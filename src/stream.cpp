extern "C" {
#include <libavutil/opt.h>
#include <libavutil/dict.h>
}

#include "log.hpp"
#include "stream.hpp"

namespace vio {

std::unique_ptr<OutputStreamData> OutputStreamData::ConfigureVideoStream(
    AVFormatContext *   fmtctx,
    enum AVCodecID      codec_id,
    const VideoConfig & cfg
) {
    int ret = 0;
    auto * ost = new OutputStreamData();
    // ost->set_config(_cfg);

    do {
        // (1) Allocate the output stream.
        ost->set_stream(avformat_new_stream(fmtctx, nullptr));
        if (!ost->stream()) {
            spdlog::error("[vio::VideoWriter]: Could not allocate stream.");
            break;
        }
        ost->stream()->id = fmtctx->nb_streams - 1;

        // (2) Fine a proper codec.
        auto * codec = avcodec_find_encoder(codec_id);
        if (!codec) {
            spdlog::error(
                "[vio::VideoWriter]: Could not find encoder for codec '{}'",
                avcodec_get_name(codec_id)
            );
            break;
        }
        if (codec->type != AVMEDIA_TYPE_VIDEO) {
            spdlog::error(
                "[vio::VideoWriter]: The codec '{}' is not for video!",
                avcodec_get_name(codec_id)
            );
            break;
        }
        ost->set_codec(codec);

        // (3) allocate context for the codec
        auto * codec_ctx = avcodec_alloc_context3(codec);
        if (!codec_ctx) {
            spdlog::error("[vio::VideoWriter]: Could not alloc an encoding context.");
            break;
        }
        ost->set_codec_ctx(codec_ctx);

#ifndef NDEBUG
        spdlog::debug("[vio::VideoWriter]: codec {} ({}) for {}",
            codec->name,
            codec->long_name,
            avcodec_get_name(codec_id)
        );
#endif

        // Configure the codec context
        {
            codec_ctx->codec_id  = codec_id;
            codec_ctx->width     = cfg.width;
            codec_ctx->height    = cfg.height;
            // Optional bitrate. 0 is not set.
            codec_ctx->bit_rate  = cfg.bitrate;
            // Optional crf for libx264.
            if (strcmp(codec->name, "libx264") == 0) {
                av_opt_set_double(codec_ctx->priv_data, "crf", cfg.crf, 0);
            }

            // NOTE: Resolution must be a multiple of two.
            if ((codec_ctx->width % 2 != 0) || (codec_ctx->height % 2 != 0)) {
                spdlog::error(
                    "[vio::VideoWriter]: Configured image size {}x{} is invalid! Must be even.",
                    codec_ctx->width, codec_ctx->height
                );
                break;
            }

            // timebase: This is the fundamental unit of time (in seconds) in terms
            // of which frame timestamps are represented. For fixed-fps content,
            // timebase should be 1/framerate and timestamp increments should be
            // identical to 1.
            codec_ctx->time_base = av_inv_q(cfg.fps);
            codec_ctx->gop_size  = cfg.g;
            codec_ctx->pix_fmt   = AV_PIX_FMT_YUV420P;
            // if (codec_ctx->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
            //     codec_ctx->max_b_frames = 0;  // just for testing, we also add B-frames
            // }
            if (codec_ctx->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
                // Needed to avoid using macroblocks in which some coeffs overflow.
                // This does not happen with normal video, it just happens here as
                // the motion of the chroma plane does not match the luma plane.
                codec_ctx->mb_decision = 2;
            }
            ost->stream()->time_base = codec_ctx->time_base;
        }

        // Some formats want stream headers to be separate.
        if (fmtctx->oformat->flags & AVFMT_GLOBALHEADER) {
            codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        // Open the codec
        ret = avcodec_open2(codec_ctx, codec, nullptr);
        if (ret < 0) {
            spdlog::error("[vio:VideoWriter]: Could not open video codec: {}", av_err2str(ret));
            break;
        }

        return std::unique_ptr<OutputStreamData>(ost);
    } while (false);

    // failure
    delete ost;
    return nullptr;
}

}
