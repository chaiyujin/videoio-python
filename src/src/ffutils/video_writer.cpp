#include "video_writer.hpp"

namespace ffutils {

std::unique_ptr<OutputStream> OutputStream::ConfigureStream(
    AVFormatContext * _oc,
    enum AVCodecID    _codec_id,
    const MediaConfig & _cfg
) {
    OutputStream * ost = new OutputStream();
    ost->set_config(_cfg);

    do {
        bool success = false;
        auto * codec = avcodec_find_encoder(_codec_id);
        if (!codec) {
            spdlog::error(
                "[ffmpeg::Writer]: Could not find encoder for '{}'",
                avcodec_get_name(_codec_id)
            );
            break;
        }
        ost->set_codec(codec);
        ost->set_stream(avformat_new_stream(_oc, nullptr));
        if (!ost->stream()) {
            spdlog::error("[ffmpeg::Writer]: Could not allocate stream.");
            break;
        }
        ost->stream()->id = _oc->nb_streams-1;
        auto * codec_ctx = avcodec_alloc_context3(codec);
        if (!codec_ctx) {
            spdlog::error("[ffmpeg::Writer]: Could not alloc an encoding context.");
            break;
        }
        ost->set_codec_ctx(codec_ctx);

        switch (codec->type) {
        case AVMEDIA_TYPE_AUDIO:
            codec_ctx->sample_fmt  = codec->sample_fmts ? codec->sample_fmts[0] : AV_SAMPLE_FMT_S16P;
            codec_ctx->bit_rate    = _cfg.audio.bitrate;
            codec_ctx->sample_rate = _cfg.audio.sample_rate;
            if (codec->supported_samplerates)
            {
                codec_ctx->sample_rate = codec->supported_samplerates[0];
                for (int i = 0; codec->supported_samplerates[i]; i++)
                {
                    if (codec->supported_samplerates[i] == _cfg.audio.sample_rate)
                        codec_ctx->sample_rate = _cfg.audio.sample_rate;
                }
                if (codec_ctx->sample_rate != _cfg.audio.sample_rate)
                {
                    spdlog::error("[ffmpeg::Writer]: sampleRate {} is not supported.",
                               _cfg.audio.sample_rate);
                    break;
                }
            }
            codec_ctx->channels = _cfg.audio.channels;
            codec_ctx->channel_layout = _cfg.audio.layout;
            if (codec->channel_layouts)
            {
                codec_ctx->channel_layout = codec->channel_layouts[0];
                for (int i = 0; codec->channel_layouts[i]; i++)
                {
                    if (codec->channel_layouts[i] == _cfg.audio.layout)
                        codec_ctx->channel_layout = _cfg.audio.layout;
                }
                if (codec_ctx->channel_layout != _cfg.audio.layout)
                {
                    spdlog::error("[ffmpeg::Writer]: channel_layout {} is not supported.",
                               av_get_channel_description(_cfg.audio.layout));
                    break;
                }
            }
            codec_ctx->time_base = AVRational{ 1, codec_ctx->sample_rate };
            ost->stream_->time_base = codec_ctx->time_base;
            success = true;
            break;

        case AVMEDIA_TYPE_VIDEO:
            codec_ctx->codec_id  = _codec_id;
            codec_ctx->bit_rate  = _cfg.video.bitrate;
            /* Resolution must be a multiple of two. */
            codec_ctx->width     = _cfg.video.resolution.x;
            codec_ctx->height    = _cfg.video.resolution.y;
            /* timebase: This is the fundamental unit of time (in seconds) in terms
            * of which frame timestamps are represented. For fixed-fps content,
            * timebase should be 1/framerate and timestamp increments should be
            * identical to 1. */
            codec_ctx->time_base = AVRational { (int)_cfg.video.fps.den, (int)_cfg.video.fps.num };
            codec_ctx->gop_size  = (int)((double)_cfg.video.fps * 5.0); /* emit one intra frame every twelve frames at most */
            codec_ctx->pix_fmt   = AV_PIX_FMT_YUV420P;
            if (codec_ctx->codec_id == AV_CODEC_ID_MPEG2VIDEO)
            {
                codec_ctx->max_b_frames = 0;  /* just for testing, we also add B-frames */
            }
            if (codec_ctx->codec_id == AV_CODEC_ID_MPEG1VIDEO)
            {
                /* Needed to avoid using macroblocks in which some coeffs overflow.
                * This does not happen with normal video, it just happens here as
                * the motion of the chroma plane does not match the luma plane. */
                codec_ctx->mb_decision = 2;
            }
            ost->stream_->time_base = codec_ctx->time_base;
            success = true;
            break;
        default:
            break;
        }

        if (!success) break;

        /* Some formats want stream headers to be separate. */
        if (_oc->oformat->flags & AVFMT_GLOBALHEADER)
            codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        return std::unique_ptr<OutputStream>(ost);
    } while (false);

    // failure
    delete ost;
    return nullptr;
}

}
