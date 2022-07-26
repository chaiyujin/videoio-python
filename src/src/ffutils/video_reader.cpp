#include "video_reader.hpp"

static size_t  MAX_FRAME_BUFFER_SIZE = 20;
static int32_t SEEKING_TRIGGER_HOP = 10;

namespace ffutils {

bool VideoReader::open(std::string _filepath, MediaConfig _cfg) {
    int ret;

    // Open input and get format context
    AVFormatContext * fmt = nullptr;
    ret = avformat_open_input(&fmt, _filepath.c_str(), NULL, NULL);
    if (ret < 0) {
        snow::log::error("[ffmeg::VideoReader]: Cannot open input file '{}': {}.",
                   _filepath, av_err2str(ret));
        return false;
    }
    fmt_ctx_.reset(fmt);

    // Find stream information
    ret = avformat_find_stream_info(fmt_ctx_.get(), NULL);
    if (ret < 0) {
        snow::log::error("[ffmeg::VideoReader]: Could not find stream information: {}.",
                   av_err2str(ret));
        this->_cleanup();
        return false;
    }

    // Open streams
    for (uint32_t i = 0; i < fmt_ctx_->nb_streams; i++) {
        // allocate decoder codec
        AVStream *      stream     = fmt_ctx_->streams[i];
        auto            codec_id   = stream->codecpar->codec_id;
        std::string     codec_name = avcodec_get_name(codec_id);
        AVCodec const * dec        = avcodec_find_decoder(codec_id);
        if (dec == nullptr) {
            snow::log::warn(
                "[ffmpeg::VideoReader]: Failed to find codec"
                " for stream {} of codec {}(id: {}).",
                i, codec_name, codec_id
            );
            input_streams_.push_back(nullptr);
            continue;
        }
        AVCodecContext * dec_ctx = avcodec_alloc_context3(dec);
        if (dec_ctx == nullptr) {
            snow::log::warn(
                "[ffmpeg::VideoReader]: Failed to allocate context"
                " for stream {} of codec {}(id: {}).",
                i, codec_name, codec_id
            );
            continue;
        }
        ret = avcodec_parameters_to_context(dec_ctx, stream->codecpar);
        if (ret < 0) {
            snow::log::warn(
                "[ffmpeg::VideoReader]: Failed to copy codec parameters"
                " to input decoder context for stream {} of codec {}(id: {})."
                " Detail: {}",
                i, codec_name, codec_id, av_err2str(ret)
            );
            continue;
        }

        // ! Ignore other codec_type
        if (dec_ctx->codec_type != AVMEDIA_TYPE_VIDEO) {
            input_streams_.push_back(nullptr);
            continue;
        }

        // set codec to automatically determine how many threads suits best for the decoding job
        dec_ctx->thread_count = 0;
        if (dec->capabilities | AV_CODEC_CAP_FRAME_THREADS)
            dec_ctx->thread_type = FF_THREAD_FRAME;
        else if (dec->capabilities | AV_CODEC_CAP_SLICE_THREADS)
            dec_ctx->thread_type = FF_THREAD_SLICE;
        else
            dec_ctx->thread_count = 1; //don't use multithreading
        // snow::log::debug("codec ctx thread: {}", dec_ctx->thread_count);

        // Open decoder
        ret = avcodec_open2(dec_ctx, dec, NULL);
        if (ret < 0) {
            snow::log::error("[ffmpeg::VideoReader]: Failed to open decoder for stream {}", i);
            this->_cleanup();
            return false;
        }

        // allocate new input stream and set
        auto config = _cfg;

        input_streams_.push_back(std::move(std::make_unique<InputStream>()));
        auto & st = input_streams_.back();
        st->set_type(dec_ctx->codec_type);
        st->set_stream(stream);
        st->set_codec(dec);
        st->set_codec_ctx(dec_ctx);
        st->n_frames_ = stream->nb_frames;  // init nb_frames, maybe not accurate

        // for specific type
        if (st->type() == AVMEDIA_TYPE_VIDEO) {
            st->video_stream_idx_ = video_streams_.size();

            // set stream config
            config.video.bitrate    = dec_ctx->bit_rate;
            config.video.resolution = (prod(config.video.resolution) == 0)
                ? int2({ (int32_t)dec_ctx->width, (int32_t)dec_ctx->height })
                : config.video.resolution;
            config.video.fps = {stream->avg_frame_rate.num, stream->avg_frame_rate.den};

            // rotation
            auto * rotateTag = av_dict_get(st->stream()->metadata, "rotate", NULL, 0);
            if (rotateTag != nullptr) { config.video.rotation = std::stoi(rotateTag->value); }

            // pix fmt
            AVPixelFormat tarPixFmt = config.video.pix_fmt;
            AVPixelFormat decPixFmt = dec_ctx->pix_fmt;

            // allocate frame buffer for decoded frames
            st->buffer().allocate(MAX_FRAME_BUFFER_SIZE, decPixFmt, dec_ctx->width, dec_ctx->height);
            // allocate temporary frame (for sws_scale)
            st->set_tmp_frame(AllocateFrame(tarPixFmt, config.video.resolution.x, config.video.resolution.y));

            // allocate scaler
            if (decPixFmt != tarPixFmt) {
                // char buf[2048];
                // av_get_pix_fmt_string(buf, 2048, decPixFmt);
                // snow::log::info("context: {}", buf);
                st->set_sws_ctx(sws_getContext(
                    dec_ctx->width,
                    dec_ctx->height,
                    decPixFmt,
                    config.video.resolution.x,
                    config.video.resolution.y,
                    tarPixFmt,
                    SWS_BICUBIC, NULL, NULL, NULL
                ));
                if (!st->sws_ctx()) {
                    snow::log::error("[ffmpeg::VideoReader]: Could not initialize the sws context");
                    this->_cleanup();
                    return false;
                }
            }

            // other things
            video_streams_.push_back(input_streams_.back().get());
        }
        st->set_config(config);
    }

    is_open_ = (video_streams_.size() > 0);
    vidx_ = 0;  // ! Only use the first track
    if (!is_open_) {
        snow::log::debug("Failed to open, cleanup");
        this->_cleanup();
    } else {
        // Get start_time, duration from fmt_ctx
        if (fmt_ctx_->start_time != AV_NOPTS_VALUE) {
            start_time_ = AVTimeToTimestamp(fmt_ctx_->start_time);
        }
        if (fmt_ctx_->duration != AV_NOPTS_VALUE) {
            duration_ = AVTimeToTimestamp(fmt_ctx_->duration);
        }
        // Get from the video stream
        auto * st = video_streams_[vidx_]->stream();
        auto & cfg = video_streams_[vidx_]->config();
        if (st->start_time != AV_NOPTS_VALUE) {
            start_time_ = AVTimeToTimestamp(st->start_time, st->time_base);
        }
        if (st->duration != AV_NOPTS_VALUE) {
            duration_ = AVTimeToTimestamp(st->duration, st->time_base);
        }
        fps_ = st->avg_frame_rate;
        tbr_ = st->r_frame_rate;

        snow::log::debug("num of video streams: {}", video_streams_.size());
        snow::log::debug("media start time {}, duration {}, fps {}, tbr {}", start_time_, duration_, fps_, tbr_);
    }

    return is_open_;
}

int VideoReader::_process_packet() {
    int ret;
    int got_frame = 0;
    AVPacket pkt = {};

    err_again_ = false;
    if (!fmt_ctx_) {
        return AVERROR_EOF;
    }

    auto _decodeFrame = [&](InputStream * st) -> int {
        if (st->type() == AVMEDIA_TYPE_VIDEO) {
            int ret = avcodec_receive_frame(st->codec_ctx(), st->frame());
            if (ret == 0) {
                got_frame = 1;
            }
            return ret;
        }
        return AVERROR(EAGAIN);
    };

    auto _copyResults = [&](InputStream * st) {
        // new frame
        st->buffer().push_back();
        AVFrame * new_frame = st->buffer().offset_back(0);
        av_frame_copy(new_frame, st->frame());
        av_frame_copy_props(new_frame, st->frame());
        new_frame->pts = new_frame->best_effort_timestamp;
        // set to new frame
        frame_ = new_frame;
    };

    // try decode first
    ret = _decodeFrame(video_streams_[0]);
    if (ret == 0 || ret == AVERROR_EOF) {
        // snow::log::warn("flush buffered frames.");
        if (got_frame) {
            _copyResults(video_streams_[0]);
        }
        return ret;
    }

    // read frame
    ret = av_read_frame(fmt_ctx_.get(), &pkt);

    // Error Again: we need to read again, just return
    if (ret == AVERROR(EAGAIN)) {
        err_again_ = true;
        av_packet_unref(&pkt);
        return AVERROR(EAGAIN);
    }

    // Error EOF
    if (ret == AVERROR_EOF) {
        snow::log::debug("  EOF!!! stream_index {}", pkt.stream_index);
        av_packet_unref(&pkt);
        // ! Don't return now, because some frames maybe cached in the decoder.
    }

    // Check the stream is the target video track
    auto * st = (ret != AVERROR_EOF)
        ? input_streams_[pkt.stream_index].get()
        : video_streams_[0];  // HACK: hard-coded the first video stream, we try to flush the cached frames.

    // ! Not fisrt video track
    if (!st || st != video_streams_[0]) {
        err_again_ = true;  // !HACK: abuse err_again_ here, it's a not needed stream
        av_packet_unref(&pkt);
        return ret;  // EOF or EAGAIN
    }

    // process frame
    if (st->type() == AVMEDIA_TYPE_VIDEO) {
        Decode(st->codec_ctx(), st->frame(), &got_frame, &pkt);

        if (got_frame) {
            _copyResults(st);
            // reset to 0
            ret = 0;
        }
    }

    if (got_frame < 1 && ret != AVERROR_EOF) {
        err_again_ = true;  // !HACK: abuse err_again
    }

    av_packet_unref(&pkt);
    return ret;
}

void VideoReader::_convert_pix_fmt() {
    // todo: which video strream?
    auto * st = video_streams_[0];
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

bool VideoReader::_read_frame() {
    if (!is_open()) {
        return false;
    }

    this->is_eof_ = false;

    bool got = false;
    do {
        int ret = this->_process_packet();
        if (ret == AVERROR_EOF) {
            this->is_eof_ = true;
            return false;
        }
        // need to decode again
        if (err_again_ && ret != AVERROR_EOF) {
            continue;
        }
        // got?
        got = (ret == 0);
    } while (!got);

    return true;
}

int32_t VideoReader::_ts_to_fidx(int64_t _timestamp) {
    if (_timestamp == AV_NOPTS_VALUE) {
        return AV_NOIDX_VALUE;
    }

    // todo: which stream
    size_t sidx = 0;
    auto * stream    = video_streams_[sidx]->stream();
    auto const & fps = video_streams_[sidx]->config().video.fps;

    auto inv_fps = AVRational{fps.den, fps.num};
    auto frame_idx = av_rescale_q(_timestamp - stream->start_time, stream->time_base, inv_fps);
    return std::max((int32_t)frame_idx, (int32_t)0);
}

int64_t VideoReader::_fidx_to_ts(int32_t _frame_idx) {
    if (_frame_idx == AV_NOIDX_VALUE) {
        return AV_NOPTS_VALUE;
    }

    // todo: which stream
    size_t sidx = 0;
    auto * stream    = video_streams_[sidx]->stream();
    auto const & fps = video_streams_[sidx]->config().video.fps;

    auto inv_fps = AVRational{fps.den, fps.num};
    auto timestamp = av_rescale_q(_frame_idx, inv_fps, stream->time_base) + stream->start_time;
    return timestamp;
}

bool VideoReader::seekTime(double _msec) {
    if (!is_open()) {
        return false;
    }

    // todo: which stream
    size_t sidx = 0;
    auto * stream  = video_streams_[sidx]->stream();
    auto timestamp = av_rescale_q((int64_t)std::round(_msec), {1, 1000}, stream->time_base);
    return this->seek(this->_ts_to_fidx(timestamp));
}

bool VideoReader::seek(int32_t _frame_idx) {
    if (!is_open()) {
        return false;
    }
    if (_frame_idx >= this->n_frames()) {
        this->is_eof_ = true;
        return false;
    }

    this->is_eof_ = false;

    size_t sidx = 0;  // todo: which stream
    auto * handle = fmt_ctx_.get();
    auto * st = video_streams_[sidx];

    // > Case 1: it's same with last frame
    if (frame_ && this->_ts_to_fidx(frame_->pts) == _frame_idx) {
        read_idx_ = _frame_idx - 1;
        return true;
    }
    
    // > Case 2: it's in frame buffer
    bool in_buffer = false;
    for (size_t i = 0; i < st->buffer().size(); ++i) {
        auto * frm = st->buffer().offset_front(i);
        if (this->_ts_to_fidx(frm->pts) == _frame_idx) {
            // snow::log::debug("find in buffer! {}, {}, {}", frm->pts, this->_ts_to_fidx(frm->pts), _frame_idx);
            in_buffer = true;
            frame_ = frm;
            break;
        }
    }

    // > Case 3: not in buffer
    if (!in_buffer) {
        auto last_idx = (frame_) ? _ts_to_fidx(frame_->pts) : -1000;

        // ! HACK: If we are close to target future frame index, don't seek any more
        if (last_idx + SEEKING_TRIGGER_HOP < _frame_idx || last_idx > _frame_idx) {
            // seek the nearest key frame
            auto * stream = video_streams_[sidx]->stream();
            auto timestamp = _fidx_to_ts(_frame_idx);
            int ret = av_seek_frame(handle, sidx, timestamp, AVSEEK_FLAG_BACKWARD);
            snow::log::debug(
                "seek frame: {}, ts: {}, {}",
                _frame_idx,
                AVTimeToTimestamp(timestamp, stream->time_base),
                _ts_to_fidx(timestamp)
            );
        } else {
            snow::log::debug("near frame: {}, {}, {}", last_idx, _frame_idx, SEEKING_TRIGGER_HOP);
        }

        // read until the right frame
        int32_t cur = -1;
        if (frame_) {
            cur = this->_ts_to_fidx(frame_->pts);
        }

        // ! cannot use 'cur < _frame_idx' to judge, due to B-frame (need future frames to decode)
        // while (cur != _frame_idx) {
        while (cur != _frame_idx) {
            snow::log::debug("  cur {}, target {}", cur, _frame_idx);
            // no frame got, eof
            bool got = this->_read_frame();
            snow::log::debug("  read new frame: got? {}", got);
            if (!got) {
                break;
            }
            cur = this->_ts_to_fidx(frame_->pts);
            snow::log::debug("  got frame at {}, {}", frame_->pts, cur);
        }
    }

    // convert pixel format of frame_
    this->_convert_pix_fmt();
    read_idx_ = _frame_idx - 1;
    return true;
}

bool VideoReader::read() {
    if (!this->is_open() || this->is_eof()) {
        return false;
    }

    int32_t new_idx = read_idx_ + 1;
    bool got = this->seek(new_idx);
    read_idx_ = new_idx;

    return got;
}

}
