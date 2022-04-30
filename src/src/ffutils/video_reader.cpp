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
        spdlog::error(
            "[ffmeg::VideoReader]: Cannot open input file '{}': {}.",
            _filepath, av_err2str(ret)
        );
        return false;
    }
    fmt_ctx_.reset(fmt);

    // Find stream information
    ret = avformat_find_stream_info(fmt_ctx_.get(), NULL);
    if (ret < 0) {
        spdlog::error(
            "[ffmeg::VideoReader]: Could not find stream information: {}.",
            av_err2str(ret)
        );
        this->_cleanup();
        return false;
    }

    // Start timestamp
    auto start_time = Timestamp(0);
    if (fmt_ctx_->start_time != AV_NOPTS_VALUE) {
        start_time = AVTimeToTimestamp(fmt_ctx_->start_time);
    }
    spdlog::debug("media start time {}", start_time);

    // Open streams
    for (uint32_t i = 0; i < fmt_ctx_->nb_streams; i++) {
        // allocate decoder codec
        AVStream *      stream     = fmt_ctx_->streams[i];
        auto            codec_id   = stream->codecpar->codec_id;
        std::string     codec_name = avcodec_get_name(codec_id);
        AVCodec const * dec        = avcodec_find_decoder(codec_id);
        if (dec == nullptr) {
            spdlog::warn(
                "[ffmpeg::VideoReader]: Failed to find codec"
                " for stream {} of codec {}(id: {}).",
                i, codec_name, codec_id
            );
            input_streams_.push_back(nullptr);
            continue;
        }
        AVCodecContext * dec_ctx = avcodec_alloc_context3(dec);
        if (dec_ctx == nullptr) {
            spdlog::warn(
                "[ffmpeg::VideoReader]: Failed to allocate context"
                " for stream {} of codec {}(id: {}).",
                i, codec_name, codec_id
            );
            continue;
        }
        ret = avcodec_parameters_to_context(dec_ctx, stream->codecpar);
        if (ret < 0) {
            spdlog::warn(
                "[ffmpeg::VideoReader]: Failed to copy codec parameters"
                " to input decoder context for stream {} of codec {}(id: {})."
                " Detail: {}",
                i, codec_name, codec_id, av_err2str(ret)
            );
            continue;
        }

        // ignore other codec_type
        if (dec_ctx->codec_type != AVMEDIA_TYPE_VIDEO) {
            input_streams_.push_back(nullptr);
            continue;
        }

        spdlog::debug("stream {}, start {}", i, stream->start_time);

        // if video
        if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
            dec_ctx->framerate = av_guess_frame_rate(fmt_ctx_.get(), stream, NULL);
            // spdlog::debug("stream {}, video, fps: {}", i, dec_ctx->framerate);
        }

        // set codec to automatically determine how many threads suits best for the decoding job
        dec_ctx->thread_count = 0;
        if (dec->capabilities | AV_CODEC_CAP_FRAME_THREADS)
            dec_ctx->thread_type = FF_THREAD_FRAME;
        else if (dec->capabilities | AV_CODEC_CAP_SLICE_THREADS)
            dec_ctx->thread_type = FF_THREAD_SLICE;
        else
            dec_ctx->thread_count = 1; //don't use multithreading
        // spdlog::debug("codec ctx thread: {}", dec_ctx->thread_count);

        // Open decoder
        ret = avcodec_open2(dec_ctx, dec, NULL);
        if (ret < 0) {
            spdlog::error("[ffmpeg::VideoReader]: Failed to open decoder for stream {}", i);
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
            config.video.fps = {dec_ctx->framerate.num, dec_ctx->framerate.den};

            // rotation
            auto * rotateTag = av_dict_get(st->stream()->metadata, "rotate", NULL, 0);
            if (rotateTag != nullptr) { config.video.rotation = std::stoi(rotateTag->value); }

            // pix fmt
            AVPixelFormat tarPixFmt = config.video.pix_fmt;
            AVPixelFormat decPixFmt;
            switch (dec_ctx->pix_fmt) {
                case AV_PIX_FMT_YUVJ420P: decPixFmt = AV_PIX_FMT_YUV420P; break;
                case AV_PIX_FMT_YUVJ422P: decPixFmt = AV_PIX_FMT_YUV422P; break;
                case AV_PIX_FMT_YUVJ444P: decPixFmt = AV_PIX_FMT_YUV444P; break;
                case AV_PIX_FMT_YUVJ440P: decPixFmt = AV_PIX_FMT_YUV440P; break;
                default:                  decPixFmt = dec_ctx->pix_fmt;
            }

            // allocate frame buffer for decoded frames
            st->buffer().allocate(MAX_FRAME_BUFFER_SIZE, decPixFmt, dec_ctx->width, dec_ctx->height);
            // allocate temporary frame (for sws_scale)
            st->set_tmp_frame(AllocateFrame(tarPixFmt, config.video.resolution.x, config.video.resolution.y));

            // allocate scaler
            if (decPixFmt != tarPixFmt) {
                // char buf[2048];
                // av_get_pix_fmt_string(buf, 2048, decPixFmt);
                // spdlog::info("context: {}", buf);
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
                    spdlog::error("[ffmpeg::VideoReader]: Could not initialize the sws context");
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
    if (!is_open_) {
        this->_cleanup();
    }

    return is_open_;
}

int VideoReader::_process_packet() {
    int ret;
    int got_frame = 0;
    AVPacket pkt;

    err_again_ = false;
    if (!fmt_ctx_) {
        return AVERROR_EOF;
    }

    // read frame
    ret = av_read_frame(fmt_ctx_.get(), &pkt);
    if (ret == AVERROR(EAGAIN)) {
        err_again_ = true;
        av_packet_unref(&pkt);
        return ret;
    }

    // end
    if (ret == AVERROR_EOF) {
        av_packet_unref(&pkt);
        return ret;
    }

    // process frame
    auto & st = input_streams_[pkt.stream_index];
    // ! Not fisrt video track
    if (!st || st.get() != video_streams_[0]) {
        err_again_ = true;  // !HACK: abuse err_again_ here, it's a not needed stream
        return ret;
    }

    // bool keyframe = !!(pkt.flags & AV_PKT_FLAG_KEY);
    // spdlog::info("keyframe? {}, {}, {}", keyframe, pkt.pts, pkt.dts);

    // video
    if (st->type() == AVMEDIA_TYPE_VIDEO) {
        Decode(st->codec_ctx(), st->frame(), &got_frame, &pkt);

        if (got_frame) {
            // new frame
            st->buffer().push_back();
            AVFrame * new_frame = st->buffer().offset_back(0);
            av_frame_copy(new_frame, st->frame());
            av_frame_copy_props(new_frame, st->frame());
            new_frame->pts = new_frame->best_effort_timestamp;
            int64_t pts = new_frame->best_effort_timestamp;

            // set to new frame
            frame_ = new_frame;
        }
    }

    if (got_frame != 1) {
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
    return frame_idx;
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
            // spdlog::debug("find in buffer! {}, {}, {}", frm->pts, this->_ts_to_fidx(frm->pts), _frame_idx);
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
            auto timestamp = _fidx_to_ts(_frame_idx);
            int ret = av_seek_frame(handle, sidx, timestamp, AVSEEK_FLAG_BACKWARD);
            // spdlog::debug("seek frame: {}, ts: {}, {}", _frame_idx, timestamp, _ts_to_fidx(timestamp));
        } else {
            // spdlog::debug("near ! frame: {}, {}, {}", last_idx, _frame_idx, SEEKING_TRIGGER_HOP);
        }

        // read until the right frame
        int32_t cur = -1;
        if (frame_) {
            cur = this->_ts_to_fidx(frame_->pts);
        }

        // ! cannot use 'cur < _frame_idx' to judge, due to B-frame (need future frames to decode)
        while (cur != _frame_idx) {
            // no frame got, eof
            if (!this->_read_frame()) {
                if (st->n_frames_ > cur + 1) {
                    st->n_frames_ = cur + 1;
                }
                break;
            }
            cur = this->_ts_to_fidx(frame_->pts);
            // spdlog::debug("  got frame at {}, {}", frame_->pts, cur);
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
    this->seek(new_idx);
    read_idx_ = new_idx;

    return true;
}

}
