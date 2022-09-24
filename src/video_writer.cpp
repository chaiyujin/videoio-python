extern "C" {
#include <libavutil/pixdesc.h>
#include <libavutil/log.h>
}
#include "video_writer.hpp"

namespace vio {

void VideoWriter::close() {
    if (isOpened()) {
        while (this->_writeVideoFrame(nullptr, 0, 0));
        av_write_trailer(fmtctx_.get());
        // close the output file.
        if (!(fmtctx_->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&fmtctx_->pb);
        }
    }
    this->_cleanup();
}

bool VideoWriter::open(std::string const & filename, VideoConfig cfg) {
    int ret = 0;
    this->close();

    // NOTE: check config.
    // Input image is rgb or rgba, video encoded with yuv420p
    AVPixelFormat pix_fmt = av_get_pix_fmt(cfg.pix_fmt.c_str());
    if (pix_fmt == AV_PIX_FMT_NONE) {
        spdlog::error("[vio::Videowriter]: pix_fmt '{}' is invalid!", cfg.pix_fmt);
        return false;
    }

    // allocate the output media context
    AVFormatContext *fmt = nullptr;
    avformat_alloc_output_context2(&fmt, nullptr, nullptr, filename.c_str());
    if (!fmt) {
        spdlog::warn("[ffmpeg::Writer]: Could not deduce output format"
                " from file extension. Trying to use 'MPEG'.");
        avformat_alloc_output_context2(&fmt, nullptr, "mpeg", filename.c_str());
    }
    if (!fmt) {
        spdlog::error("[ffmpeg::Writer]: Could not allocate format context!");
        return false;
    }
    this->fmtctx_ = std::unique_ptr<AVFormatContext, void(*)(AVFormatContext *)>(
        fmt, [](AVFormatContext * p) { avformat_free_context(p); }
    );

    // av_dump_format(fmt, 0, filename_.c_str(), 1);

    // Open file
    if (!(fmt->oformat->flags & AVFMT_NOFILE)) {
        int ret = avio_open(&fmt->pb, filename.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            spdlog::error(
                "[vio::VideoWriter]: Could not open '{}'! Detail: {}",
                filename, av_err2str(ret)
            );
            this->_cleanup();
            return false;
        }
    }

    // Allocate and configure the video stream
    video_stream_data_ = OutputStreamData::ConfigureVideoStream(fmt, fmt->oformat->video_codec, cfg);
    if (video_stream_data_ == nullptr) {
        this->_cleanup();
        return false;
    }
    auto * ost = video_stream_data_.get();
    auto * codec_ctx = ost->codec_ctx();

    /* allocate and init a re-usable frame */
    ost->set_frame(AllocateFrame(codec_ctx->pix_fmt, codec_ctx->width, codec_ctx->height));
    if (!ost->frame()) {
        spdlog::error("[vio::VideoWriter]: Could not allocate video frame.");
        this->_cleanup();
        return false;
    }

    ost->set_tmp_frame(AllocateFrame(pix_fmt, codec_ctx->width, codec_ctx->height));
    if (!ost->tmp_frame()) {
        spdlog::error("[vio::VideoWriter]: Could not allocate temporary picture.");
        this->_cleanup();
        return false;
    }

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->stream()->codecpar, codec_ctx);
    if (ret < 0) {
        spdlog::error("[vio::VideoWriter]: Could not copy the stream parameters."
                    " Detail: {}", av_err2str(ret));
        this->_cleanup();
        return false;
    }

    // get convert
    ost->set_sws_ctx(sws_getContext(
        codec_ctx->width, codec_ctx->height, pix_fmt,
        codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
        SWS_BICUBIC, NULL, NULL, NULL
    ));
    if (!ost->sws_ctx()) {
        spdlog::error("[vio::VideoWriter]: Could not initialize the conversion context.");
        this->_cleanup();
        return false;
    }

    // Optional: Write the stream header, if any.
    ret = avformat_write_header(fmt, nullptr);
    if (ret < 0) {
        spdlog::error("Error occurred when opening output file: {}",
                   av_err2str(ret));
        this->_cleanup();
        return false;
    }

    this->video_config_ = cfg;
    return true;
}

// * -------------------------------------------------------------------------------------------------------------- * //
// *                                                   write frame                                                  * //
// * -------------------------------------------------------------------------------------------------------------- * //

#ifndef NDEBUG
static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt) {
    AVRational time_base = fmt_ctx->streams[pkt->stream_index]->time_base;
    spdlog::debug(
        "pts: {} pts_time: {} dts: {} dts_time: {} duration: {} duration_time: {} stream_index: {}",
        pkt->pts, av_ts2timestr(pkt->pts, time_base),
        pkt->dts, av_ts2timestr(pkt->dts, time_base),
        pkt->duration, av_ts2timestr(pkt->duration, time_base),
        pkt->stream_index
    );
}
#endif

bool VideoWriter::_writeVideoFrame(const uint8_t * data, uint32_t linesize, uint32_t height) {
    AVFrame * frame = nullptr;
    auto * ost = video_stream_data_.get();
    auto * codec_ctx = video_stream_data_->codec_ctx();

    if (data != nullptr) {
        // Convert image
        int ret = av_frame_make_writable(ost->frame());
        if (ret < 0) {
            spdlog::error(
                "[vio::VideoWriter]: Cannot make video frame writable!"
                " Detail: {}", av_err2str(ret)
            );
            return false;
        }

        // Copy data !!!!!
        size_t copy_bytes = linesize;
        if ((ost->tmp_frame()->linesize[0] > 0) && ((uint32_t)ost->tmp_frame()->linesize[0] < linesize)) {
            copy_bytes = static_cast<size_t>(ost->tmp_frame()->linesize[0]);
        }
        for (uint32_t y = 0; y < height; ++y) {
            uint8_t const * src = data + y * linesize;
            uint8_t       * dst = ost->tmp_frame()->data[0] + y * ost->tmp_frame()->linesize[0];
            memcpy(dst, src, copy_bytes);
        }

        // Sws scale
        sws_scale(ost->sws_ctx(), (const uint8_t * const *)ost->tmp_frame()->data, ost->tmp_frame()->linesize,
                    0, codec_ctx->height, ost->frame()->data, ost->frame()->linesize);
        ost->frame()->pts = ost->next_pts();
        // NOTE: increasing next pts. (time_base is inv_fps.)
        ost->set_next_pts(ost->next_pts() + 1);
        frame = ost->frame();
    }

    auto _getAndWritePacket = [&](bool flush_all_packets) -> bool {
        int ret = 0;
        bool written = false;
        do {
            AVPacket pkt = {};
            ret = avcodec_receive_packet(codec_ctx, &pkt);
#ifndef NDEBUG
            spdlog::debug("  (avcodec_receive_packet): {}, {}", ret, av_err2str(ret));
#endif
            if (ret == 0) {
                // Rescale output packet timestamp values from codec to stream timebase.
                av_packet_rescale_ts(&pkt, codec_ctx->time_base, ost->stream()->time_base);
                // NOTE: It's important to set duration, so that fps is the same as tbr.
                pkt.duration = av_rescale_q(1, codec_ctx->time_base, ost->stream()->time_base);
                pkt.stream_index = ost->stream()->index;
                // Write the compressed frame to the media file.
#ifndef NDEBUG
                log_packet(fmtctx_.get(), &pkt);
#endif
                ret = av_interleaved_write_frame(fmtctx_.get(), &pkt);
                /* pkt is now blank (av_interleaved_write_frame() takes ownership of
                * its contents and resets pkt), so that no unreferencing is necessary.
                * This would be different if one used av_write_frame(). */
                if (ret < 0) {
                    spdlog::error("[vio::VideoWriter]: Failed to write video frame: {}", av_err2str(ret));
                    return false;
                }
                written = true;
                if (!flush_all_packets) {
                    return true;
                }
            }
            else if (ret == AVERROR_EOF) {
                break;
            }
            else if (ret == AVERROR(EAGAIN)) {
                return written;
            }
            else {
                spdlog::error("[vio::VideoWriter]: impossible error for avcodec_receive_packet: {}", av_err2str(ret));
                return false;
            }
        } while (ret >= 0);
        return written;
    };

    // Send the frame to codec.
    do {
        int ret = avcodec_send_frame(codec_ctx, frame);
        bool flush_all_packets = frame == nullptr;
#ifndef NDEBUG
        spdlog::debug("  (avcodec_send_frame): {}, {}", ret, av_err2str(ret));
#endif
        // flush_all_packets = true;

        if (ret == AVERROR(EAGAIN)) {
            // Some packets need to be read.
            _getAndWritePacket(true);
        }
        else if (ret == AVERROR_EOF) {
#ifndef NDEBUG
            spdlog::debug("[vio::VideoWriter]: encoder codec EOF.");
#endif
            return _getAndWritePacket(true);
        }
        else if (ret == AVERROR(EINVAL)) {
            // codec not opened, it is a decoder, or requires flush
            // It's ensure that codec is opened and encoder here, so we flush the codec.
            avcodec_send_frame(codec_ctx, nullptr);
            _getAndWritePacket(flush_all_packets);
        }
        else if (ret == 0) {
            return _getAndWritePacket(flush_all_packets);
        }
        else {
            spdlog::error("[vio::VideoWriter]: encoder codec failed! {}", av_err2str(ret));
            return false;
        }
    } while (true);

    return false;
}

}
