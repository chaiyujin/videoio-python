#include "log.hpp"
#include "video_reader.hpp"

static int32_t AV_NOIDX_VALUE = (int32_t)UINT32_C(0x80000000);
static size_t  MAX_FRAME_BUFFER_SIZE = 20;
static int32_t SEEKING_TRIGGER_HOP = 10;

namespace vio {

// * -------------------------------------------------------------------------------------------------------------- * //
// *                                                  Open / Close                                                  * //
// * -------------------------------------------------------------------------------------------------------------- * //

void VideoReader::close() {
    this->_cleanup();
}

bool VideoReader::open(std::string const & filename, std::pair<int32_t, int32_t> const & target_resolution) {
    this->close();  // make sure everything is cleaned up.

    // 1. Create a file IO.
    // TODO: handle file io error?
    this->ioctx_ = std::unique_ptr<AVIOBase>(new AVFileIOContext(filename));

    // 2. Create a reading format context.
    this->fmtctx_ = std::unique_ptr<AVFormatContext, void(*)(AVFormatContext *)>(
        avformat_alloc_context(), [](AVFormatContext * p) { avformat_free_context(p); }
    );
    auto * fmt = this->fmtctx_.get();
    this->ioctx_->associateFormatContext(fmt);

    // 3. Open
    int ret = avformat_open_input(&fmt, NULL, NULL, NULL);
    if (ret < 0) {
        spdlog::error(
            "[vio::VideoReader]: Cannot open input file '{}': {}.",
            filename, av_err2str(ret)
        );
        this->_cleanup();
        return false;
    }

    // 4. Find stream information
    ret = avformat_find_stream_info(fmt, NULL);
    if (ret < 0) {
        spdlog::error(
            "[vio::VideoReader]: Could not find stream information: {}.",
            av_err2str(ret)
        );
        this->_cleanup();
        return false;
    }

    // av_dump_format(fmt, 0, filename.c_str(), 0);

    // 5. Try to initialize some properties
    if (fmt->start_time != AV_NOPTS_VALUE) { this->start_time_ = AVTime2MS(fmt->start_time); }
    if (fmt->duration   != AV_NOPTS_VALUE) { this->duration_   = AVTime2MS(fmt->duration  ); }

    if (!this->_findMainStream(target_resolution)) {
        this->_cleanup();
        return false;
    }

    // Seeking information
    seek_to_pts_ = (fmtctx_->iformat->flags & AVFMT_SEEK_TO_PTS) != 0;
    dts_pts_delta_ = 0;

#ifndef NDEBUG
    spdlog::debug(
        "start_time: {}, duration: {}, fps {}, tbr {}, {} frames. SEEK_TO_PTS={}",
        start_time_, duration_, fps_, tbr_, this->numFrames(), seek_to_pts_
    );
#endif
    return true;
}

bool VideoReader::_findMainStream(std::pair<int32_t, int32_t> const & target_resolution) {
    int ret = 0;
    auto * fmt = this->fmtctx_.get();

    // Find the first video stream
    for (size_t i = 0; i < fmt->nb_streams; i++) {
        AVStream *       stream     = fmt->streams[i];
        auto             codec_id   = stream->codecpar->codec_id;
        std::string      codec_name = avcodec_get_name(codec_id);
        AVCodec const *  codec      = avcodec_find_decoder(codec_id);

        auto sd = std::make_unique<InputStreamData>();
        sd->set_stream(stream);
        sd->set_codec(codec);

        // (1) Try to allocate the codec content.
        AVCodecContext * codec_ctx = avcodec_alloc_context3(codec);
        if (codec_ctx == nullptr) {
            spdlog::warn(
                "[vio::VideoReader]: Failed to allocate context"
                " for stream {} of codec {}(id: {}).",
                i, codec_name, codec_id
            );
            continue;
        }
        sd->set_codec_ctx(codec_ctx);

        // (2) Try to update the parameters to the codec context.
        ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
        if (ret < 0) {
            spdlog::warn(
                "[vio::VideoReader]: Failed to copy codec parameters"
                " to input decoder context for stream {} of codec {}(id: {})."
                " Detail: {}",
                i, codec_name, codec_id, av_err2str(ret)
            );
            continue;
        }

        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {

            // set codec to automatically determine how many threads suits best for the decoding job
            {
                codec_ctx->thread_count = 0;
                if      (codec->capabilities | AV_CODEC_CAP_FRAME_THREADS) codec_ctx->thread_type = FF_THREAD_FRAME;
                else if (codec->capabilities | AV_CODEC_CAP_SLICE_THREADS) codec_ctx->thread_type = FF_THREAD_SLICE;
                else                                                       codec_ctx->thread_count = 1; // single thread
#ifndef NDEBUG
                spdlog::debug("codec ctx thread: {}", codec_ctx->thread_count);
#endif
            }

            // Open decoder
            ret = avcodec_open2(codec_ctx, codec, NULL);
            if (ret < 0) {
                spdlog::error("[vio::VideoReader]: Failed to open decoder for stream {}", i);
                return false;
            }

            // pix fmt
            AVPixelFormat dec_pix_fmt = codec_ctx->pix_fmt;
            AVPixelFormat tar_pix_fmt = AV_PIX_FMT_RGB24;
            int target_width  = (target_resolution.first  == 0) ? codec_ctx->width  : target_resolution.first;
            int target_height = (target_resolution.second == 0) ? codec_ctx->height : target_resolution.second;

            // allocate frame buffer for decoded frames
            sd->buffer().allocate(MAX_FRAME_BUFFER_SIZE, dec_pix_fmt, codec_ctx->width, codec_ctx->height);
            // allocate temporary frame (for sws_scale)
            sd->set_tmp_frame(AllocateFrame(tar_pix_fmt, target_width, target_height));
            sd->image_size().first = target_width;
            sd->image_size().second = target_height;

            // allocate scaler
            if ((dec_pix_fmt != tar_pix_fmt) ||
                (target_width != codec_ctx->width) ||
                (target_height != codec_ctx->height)) {
                // char buf[2048];
                // av_get_pix_fmt_string(buf, 2048, decPixFmt);
                // snow::log::info("context: {}", buf);
                sd->set_sws_ctx(sws_getContext(
                    codec_ctx->width, codec_ctx->height, dec_pix_fmt,
                    target_width, target_height, tar_pix_fmt,
                    SWS_BICUBIC, NULL, NULL, NULL
                ));
                if (!sd->sws_ctx()) {
                    spdlog::error("[vio::VideoReader]: Could not initialize the sws context");
                    return false;
                }
            }

            // stream assignment
            main_stream_idx_ = i;
            main_stream_data_ = std::move(sd);
            fps_ = stream->avg_frame_rate;
            tbr_ = stream->r_frame_rate;
            if (stream->start_time != AV_NOPTS_VALUE) { start_time_ = AVTime2MS(stream->start_time, stream->time_base); }
            if (stream->duration   != AV_NOPTS_VALUE) { duration_   = AVTime2MS(stream->duration,   stream->time_base); }
            break;
        }
    }

    // HACK: for media files with broken metadata, we try to find a duration.
    // if (duration_ == kNoTimestamp) {
    // if (true) {
    //     for (size_t i = 0; i < fmt->nb_streams; i++) {
    //         AVStream * stream = fmt->streams[i];
    //         spdlog::debug("{}, {}, {}, {}", stream->start_time, stream->duration, stream->nb_frames, stream->codecpar->codec_type);
    //         if (duration_ == kNoTimestamp && stream->duration != AV_NOPTS_VALUE) {
    //             duration_ = AVTime2MS(stream->duration,   stream->time_base);
    //             break;
    //         }
    //     }
    // }

    return true;
}

// * -------------------------------------------------------------------------------------------------------------- * //
// *                                             Pixel Format Conversion                                            * //
// * -------------------------------------------------------------------------------------------------------------- * //


void VideoReader::_convertPixFmt() {
    auto & st = main_stream_data_;
    if (st->sws_ctx() && frame_ != st->tmp_frame()) {
        // Timeit _("sws_scale");
        sws_scale(st->sws_ctx(),
                    (const uint8_t * const *)frame_->data,
                    frame_->linesize, 0,
                    frame_->height,
                    st->tmp_frame()->data,
                    st->tmp_frame()->linesize);
        st->tmp_frame()->pts = frame_->pts;
        frame_ = st->tmp_frame();
    }
}

// * -------------------------------------------------------------------------------------------------------------- * //
// *                                 Seeking (Also support frame-by-frame reanding)                                 * //
// * -------------------------------------------------------------------------------------------------------------- * //


int32_t VideoReader::_ts_to_fidx(int64_t av_time) const {
    if (av_time == AV_NOPTS_VALUE) {
        return AV_NOIDX_VALUE;
    }

    auto * stream = main_stream_data_->stream();
    auto frame_idx = av_rescale_q(av_time - stream->start_time, stream->time_base, av_inv_q(stream->r_frame_rate));
    return std::max((int32_t)frame_idx, (int32_t)0);
}

int64_t VideoReader::_fidx_to_ts(int32_t frame_idx) const {
    if (frame_idx == AV_NOIDX_VALUE) {
        return AV_NOPTS_VALUE;
    }

    auto * stream = main_stream_data_->stream();
    auto timestamp = av_rescale_q(frame_idx, av_inv_q(stream->r_frame_rate), stream->time_base) + stream->start_time;
    return timestamp;
}

bool VideoReader::seekByFrame(int32_t frame_idx) {
    if (!isOpened()) {
        return false;
    }

    auto & st = main_stream_data_;

    // > Case 1: it's same with last frame
    if (frame_ && this->_ts_to_fidx(frame_->pts) == frame_idx) {
        read_idx_ = frame_idx - 1;
        return true;
    }

    // > Case 2: it's in frame buffer
    bool in_buffer = false;
    for (size_t i = 0; i < st->buffer().size(); ++i) {
        auto * frm = st->buffer().offset_front(i);
        if (this->_ts_to_fidx(frm->pts) == frame_idx) {
#ifndef NDEBUG
            spdlog::debug("find in buffer! {}, {}, {}", frm->pts, this->_ts_to_fidx(frm->pts), frame_idx);
#endif
            in_buffer = true;
            frame_ = frm;
            break;
        }
    }

    // > Case 3: not in buffer
    if (!in_buffer) {
        auto last_idx = (frame_) ? this->_ts_to_fidx(frame_->pts) : -1000;

        // ! HACK: If we are close to target future frame index, don't seek.
        if (last_idx + SEEKING_TRIGGER_HOP < frame_idx || last_idx > frame_idx) {
            // seek the nearest key frame
            auto * stream = st->stream();
            auto pts = _fidx_to_ts(frame_idx);
            // Seek back to keyframe
            if (this->_seekToPTS()) {
                avcodec_flush_buffers(st->codec_ctx());
                av_seek_frame(fmtctx_.get(), stream->index, pts, AVSEEK_FLAG_BACKWARD);
#ifndef NDEBUG
                spdlog::debug(
                    "seek frame: {}, pts: {}, {}",
                    frame_idx,
                    AVTime2MS(pts, stream->time_base),
                    _ts_to_fidx(pts)
                );
#endif
            }
            else {
                do {
                    auto dts = pts + dts_pts_delta_;
                    avcodec_flush_buffers(st->codec_ctx());
                    av_seek_frame(fmtctx_.get(), stream->index, dts, AVSEEK_FLAG_BACKWARD);
                    if (!this->_getFrame()) { return false; } // eof
#ifndef NDEBUG
                    spdlog::debug("seek frame: {}, guess dts: {}, want pts {}, got pts {}",
                        frame_idx,
                        AVTime2MS(dts, stream->time_base),
                        AVTime2MS(pts, stream->time_base),
                        AVTime2MS(frame_->pts, stream->time_base)
                    );
#endif
                    // Good dts
                    if (this->_ts_to_fidx(frame_->pts) <= frame_idx) {
                        break;
                    }
                    // Update the dts_pts_delta guessing.
                    dts_pts_delta_ += pts - frame_->pts;
                } while (true);
            }
        }
        else {
#ifndef NDEBUG
            spdlog::debug("near frame: {}, {}, {}", last_idx, frame_idx, SEEKING_TRIGGER_HOP);
#endif
        }

        // read until the right frame
        int32_t cur = -1;
        if (frame_) {
            cur = this->_ts_to_fidx(frame_->pts);
        }

        // ! cannot use 'cur < frame_idx' to judge, due to B-frame (need future frames to decode)
        // while (cur != frame_idx) {
        while (cur != frame_idx) {
            // No frame got.
            if (!this->_getFrame()) {
                return false;
            }
            cur = this->_ts_to_fidx(frame_->pts);
#ifndef NDEBUG
            spdlog::debug("  got frame at {}, {}", AVTime2MS(frame_->pts, st->stream()->time_base), cur);
            Timestamp pts(0);
            if (frame_) { pts = AVTime2MS(frame_->pts, st->stream()->time_base); }
            spdlog::debug("  cur {} ({}), target {}", cur, pts,  frame_idx);
#endif
        }
    }

    // convert pixel format of frame_
    this->_convertPixFmt();
    read_idx_ = frame_idx - 1;
    return true;
}

bool VideoReader::seekByTime(Millisecond ms) {
    if (!isOpened()) {
        return false;
    }

    auto * stream = main_stream_data_->stream();
    auto timestamp = av_rescale_q(ms.count(), {1, 1000}, stream->time_base);
    return this->seekByFrame(this->_ts_to_fidx(timestamp));
}

bool VideoReader::read() {
    if (!this->isOpened()) {
        return false;
    }

    int32_t new_idx = read_idx_ + 1;
    bool got = this->seekByFrame(new_idx);
    read_idx_ = new_idx;

    return got;
}

// * -------------------------------------------------------------------------------------------------------------- * //
// *                                                    Decoding                                                    * //
// * -------------------------------------------------------------------------------------------------------------- * //

int VideoReader::_readPacket(AVPacket * pkt) {
    int ret = av_read_frame(fmtctx_.get(), pkt);
    switch (ret) {
    case AVERROR(EAGAIN): break;
    case AVERROR_EOF:
#ifndef NDEBUG
        spdlog::debug("  (_readPacket) file EOF.");
#endif
        break;
    default:
        if (pkt->stream_index != (int)main_stream_idx_) {
            ret = AVERROR(EAGAIN);  // HACK: abuse EAGAIN to ignore other streams.
        }
        break;
    }

    // NOTE: Update the dts_pts_delta, which is used to guess dts for SEEK_TO_DTS.
    if ((ret == 0) && (!seek_to_pts_)) {
        auto new_delta = pkt->dts - pkt->pts;
        if (new_delta < dts_pts_delta_) {
            dts_pts_delta_ = new_delta;
        }
        // spdlog::debug("dts_pts_delta: {}", dts_pts_delta_);
    }

    return ret;
}

bool VideoReader::_getFrame() {
    auto & st = main_stream_data_;
    auto & pkt = main_stream_data_->packet();
    auto * codec_ctx = main_stream_data_->codec_ctx();

    auto _decodeFrame = [&]() -> int {
        int ret = avcodec_receive_frame(codec_ctx, st->frame());
        if (ret == 0) {
            // new frame
            st->buffer().push_back();
            AVFrame * new_frame = st->buffer().offset_back(0);
            av_frame_copy(new_frame, st->frame());
            av_frame_copy_props(new_frame, st->frame());
            new_frame->pts = new_frame->best_effort_timestamp;
            // set to new frame
            frame_ = new_frame;
        }
#ifndef NDEBUG
        else {
            spdlog::debug("  (avcodec_receive_frame) {}.", av_err2str(ret));
        }
#endif
        return ret;
    };

    do {
        // Read new a packet from file if necessary.
        int ret = 0;
        if (pkt.size == 0) {
            ret = AVERROR(EAGAIN);
            while (ret == AVERROR(EAGAIN)) {
                ret = _readPacket(&pkt);
            }
        }

        // Eof for packet reading.
        if (ret == AVERROR_EOF) {
            av_packet_unref(&pkt);
            avcodec_send_packet(codec_ctx, nullptr);
            return _decodeFrame() == 0;
        }
        // Normal case.
        else if (ret == 0) {
            switch (avcodec_send_packet(codec_ctx, &pkt)) {
                case 0: {
                    av_packet_unref(&pkt);
                    int code = _decodeFrame();
                    if      (code == 0) { return true; }
                    else if (code != AVERROR(EAGAIN)) { return false; }  // Need more input packet.
                    break;
                }
                case AVERROR(EAGAIN): { // input is not accepted in the current state - user
                                    // must read output with avcodec_receive_frame() (once
                                    // all output is read, the packet should be resent, and
                                    // the call will not fail with EAGAIN).
                    // NOTE: the packet is not unref in this case.
#ifndef NDEBUG
                    spdlog::debug("  (avcodec_send_packet) EAGAIN.");
#endif
                    return _decodeFrame() == 0;
                    break;
                }
                case AVERROR(EINVAL): { // codec not opened, it is an encoder, or requires flush
                    av_packet_unref(&pkt);
                    avcodec_send_packet(codec_ctx, nullptr);
#ifndef NDEBUG
                    spdlog::debug("  (avcodec_send_packet) EINVAL.");
#endif
                    return _decodeFrame() == 0;
                    break;
                }
                case AVERROR_EOF: { // codec is flushed.
                    av_packet_unref(&pkt);
#ifndef NDEBUG
                    spdlog::debug("  (avcodec_send_packet) EOF. Codec is flushed!");
#endif
                    return _decodeFrame() == 0;
                    break;
                }
                // case AVERROR(ENOMEM):
                default: {
                    av_packet_unref(&pkt);
                    spdlog::error("[vio::VideoReader] Failed to send packet to codec!");
                    return false;
                }
            }
        }
        else {
            spdlog::error("  (_readPacket) {}", av_err2str(ret));
            exit(1);
        }
    } while (true);

    return false;
}

}
