#include "read_video.hpp"

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
        spdlog::warn("codec ctx thread: {}", dec_ctx->thread_count);

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

        // for specific type
        if (st->type() == AVMEDIA_TYPE_VIDEO) {
            st->video_stream_idx_ = video_streams_.size();

            // set stream config
            config.video.bitrate    = dec_ctx->bit_rate;
            config.video.resolution = (prod(config.video.resolution) == 0)
                ? int2({ (int32_t)dec_ctx->width, (int32_t)dec_ctx->height })
                : config.video.resolution;
            config.video.fps = {dec_ctx->framerate.num, dec_ctx->framerate.den};
            config.video.bpp = (config.video.bpp == 0) ? 3 : config.video.bpp;

            // rotation
            auto * rotateTag = av_dict_get(st->stream()->metadata, "rotate", NULL, 0);
            if (rotateTag != nullptr) { config.video.rotation = std::stoi(rotateTag->value); }

            // pix fmt
            AVPixelFormat PixelFormat;
            switch (config.video.bpp) {
            case 3: PixelFormat = AV_PIX_FMT_RGB24; break;
            case 4: PixelFormat = AV_PIX_FMT_RGBA;  break;
            default:
                spdlog::error("[ffmpeg::VideoReader]: unknown bpp", config.video.bpp);
            }

            st->set_tmp_frame(AllocateFrame(
                PixelFormat,
                config.video.resolution.x,
                config.video.resolution.y
            ));

            AVPixelFormat decPixFmt;
            switch (dec_ctx->pix_fmt) {
            case AV_PIX_FMT_YUVJ420P: decPixFmt = AV_PIX_FMT_YUV420P; break;
            case AV_PIX_FMT_YUVJ422P: decPixFmt = AV_PIX_FMT_YUV422P; break;
            case AV_PIX_FMT_YUVJ444P: decPixFmt = AV_PIX_FMT_YUV444P; break;
            case AV_PIX_FMT_YUVJ440P: decPixFmt = AV_PIX_FMT_YUV440P; break;
            default:                  decPixFmt = dec_ctx->pix_fmt;
            }

            // allocate scaler
            if (decPixFmt != PixelFormat) {
                // char buf[2048];
                // av_get_pix_fmt_string(buf, 2048, PixelFormat);
                // spdlog::info("context: {}", buf);
                st->set_sws_ctx(sws_getContext(
                    dec_ctx->width,
                    dec_ctx->height,
                    decPixFmt,
                    config.video.resolution.x,
                    config.video.resolution.y,
                    PixelFormat,
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
            // video_queues_.push_back(std::make_shared<_QueueImage>());
            // video_queues_stream_indices_.push_back(i);
            // image_caches_.emplace_back();
        }
        st->set_config(config);

        // // ! HACK: only first video track
        // if (video_streams_.size() == 1) {
        //     break;
        // }
    }

    is_open_ = (video_streams_.size() > 0);
    return is_open_;
}

int VideoReader::_processInput() {
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
            int64_t pts = st->frame()->best_effort_timestamp;
            frame_ = st->frame();
            if (st->sws_ctx()) {
                // Timeit _("sws_scale");
                sws_scale(st->sws_ctx(),
                          (const uint8_t * const *)st->frame()->data,
                          st->frame()->linesize, 0,
                          st->frame()->height,
                          st->tmp_frame()->data,
                          st->tmp_frame()->linesize);
                frame_ = st->tmp_frame();
            }
            frame_->pts = pts;
            // spdlog::debug(
            //     "frame linesize: {}, width: {}, widthx4: {}, pts: {}",
            //     frame_->linesize[0],
            //     frame_->width,
            //     frame_->width*4,
            //     frame_->pts
            // );
            // spdlog::debug("st {} (video): pts {}", pkt.stream_index, pts);
        }
    }
    
    if (got_frame != 1) {
        err_again_ = true;  // !HACK: abuse err_again
    }

    av_packet_unref(&pkt);
    return ret;
}

bool VideoReader::read() {
    if (!is_open()) {
        return false;
    }

    bool got = false;
    do {
        int ret = this->_processInput();
        if (ret == AVERROR_EOF) {
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

int32_t VideoReader::_framePtsToIndex(int64_t _timestamp) {
    // todo: which stream
    size_t sidx = 0;
    auto * stream    = video_streams_[sidx]->stream();
    auto const & fps = video_streams_[sidx]->config().video.fps;

    auto inv_fps = AVRational{fps.den, fps.num};
    auto frame_idx = av_rescale_q(_timestamp - stream->start_time, stream->time_base, inv_fps);
    return frame_idx;
}

int32_t VideoReader::_timestampToFrameIndex(int64_t _timestamp) {
    // todo: which stream
    size_t sidx = 0;
    auto * stream    = video_streams_[sidx]->stream();
    auto const & fps = video_streams_[sidx]->config().video.fps;

    auto inv_fps = AVRational{fps.den, fps.num};
    auto frame_idx = av_rescale_q(_timestamp - stream->start_time, stream->time_base, inv_fps);
    return frame_idx;
}

int64_t VideoReader::_frameIndexToTimestamp(int32_t _frame_idx) {
    // todo: which stream
    size_t sidx = 0;
    auto * stream    = video_streams_[sidx]->stream();
    auto const & fps = video_streams_[sidx]->config().video.fps;

    auto inv_fps = AVRational{fps.den, fps.num};
    auto timestamp = av_rescale_q(_frame_idx, inv_fps, stream->time_base) + stream->start_time;
    return timestamp;
}

bool VideoReader::seek(int32_t _frame_idx) {
    if (!is_open()) {
        return false;
    }

    // todo: which stream
    size_t sidx = 0;
    auto * handle = fmt_ctx_.get();

    // get fps
    auto timestamp = _frameIndexToTimestamp(_frame_idx);
    spdlog::debug("seek frame: {}, ts: {}, {}", _frame_idx, timestamp, _timestampToFrameIndex(timestamp));

    int ret = av_seek_frame(handle, sidx, timestamp, AVSEEK_FLAG_BACKWARD);
    spdlog::debug("seek? {}", ret);

    return false;
}

}
