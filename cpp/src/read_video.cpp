#include "read_video.hpp"
#include "timer.hpp"

static void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
    FILE *pFile;
    char szFilename[32];
    int  y;
    
    // Open file
    sprintf(szFilename, "hello/frame%d.ppm", iFrame);

    pFile=fopen(szFilename, "wb");
    if(pFile==NULL)
        return;
    
    // Write header
    fprintf(pFile, "P6\n%d %d\n255\n", width, height);
    
    // Write pixel data
    for(y=0; y<height; y++)
        fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);
    
    // Close file
    fclose(pFile);
}

bool Reader::open(std::string _filepath, Config _cfg) {
    int ret;

    // Open input and get format context
    AVFormatContext * fmt = nullptr;
    ret = avformat_open_input(&fmt, _filepath.c_str(), NULL, NULL);
    if (ret < 0) {
        spdlog::error(
            "[ffmeg::Reader]: Cannot open input file '{}': {}.",
            _filepath, av_err2str(ret)
        );
        return false;
    }
    fmt_ctx_.reset(fmt);

    // Find stream information
    ret = avformat_find_stream_info(fmt_ctx_.get(), NULL);
    if (ret < 0) {
        spdlog::error(
            "[ffmeg::Reader]: Could not find stream information: {}.",
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
    int n_video_streams = 0;
    int n_audio_streams = 0;
    for (uint32_t i = 0; i < fmt_ctx_->nb_streams; i++) {
        // allocate decoder codec
        AVStream *      stream     = fmt_ctx_->streams[i];
        auto            codec_id   = stream->codecpar->codec_id;
        std::string     codec_name = avcodec_get_name(codec_id);
        AVCodec const * dec        = avcodec_find_decoder(codec_id);
        if (dec == nullptr) {
            spdlog::warn(
                "[ffmpeg::Reader]: Failed to find codec"
                " for stream {} of codec {}(id: {}).",
                i, codec_name, codec_id
            );
            input_streams_.push_back(nullptr);
            continue;
        }
        AVCodecContext * dec_ctx = avcodec_alloc_context3(dec);
        if (dec_ctx == nullptr) {
            spdlog::warn(
                "[ffmpeg::Reader]: Failed to allocate context"
                " for stream {} of codec {}(id: {}).",
                i, codec_name, codec_id
            );
            continue;
        }
        ret = avcodec_parameters_to_context(dec_ctx, stream->codecpar);
        if (ret < 0) {
            spdlog::warn(
                "[ffmpeg::Reader]: Failed to copy codec parameters"
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
            spdlog::error("[ffmpeg::Reader]: Failed to open decoder for stream {}", i);
            this->_cleanup();
            return false;
        }

        // allocate new input stream and set
        auto config = _cfg;
        auto st = std::make_unique<InputStream>();
        st->set_type(dec_ctx->codec_type);
        st->set_stream(stream);
        st->set_codec(dec);
        st->set_codec_ctx(dec_ctx);

        // for specific type
        if (st->type() == AVMEDIA_TYPE_VIDEO) {
            st->video_stream_idx_ = n_video_streams++;

            // video_queues_.push_back(std::make_shared<_QueueImage>());
            // video_queues_stream_indices_.push_back(i);
            // image_caches_.emplace_back();

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
                spdlog::error("[ffmpeg::Reader]: unknown bpp", config.video.bpp);
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
                    spdlog::error("[ffmpeg::Reader]: Could not initialize the sws context");
                    this->_cleanup();
                    return false;
                }
            }
        }

        st->set_config(config);
        input_streams_.push_back(std::move(st));
        // last_pts_list_.push_back(kNoTimestamp);
    }
    // // dump format
    // if (_dump_format)
    // {
    //     av_dump_format(fmt_ctx_.get(), 0, _filepath.c_str(), 0);
    // }

    // duration_ = AVTimeToTimestamp(fmt_ctx_.get()->duration);

    // spdlog::assertion(video_queues_.size() <= 1, "[ffmpeg::Reader]: only support at most 1 video track.");
    // spdlog::assertion(audio_queues_.size() <= 1, "[ffmpeg::Reader]: only support at most 1 audio track.");

    is_open_ = true;
    return true;
}

int Reader::_processInput() {
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
    if (!st) {
        err_again_ = true;  // !HACK: abuse err_again_ here, it's a not needed stream
        return ret;
    }

    bool keyframe = !!(pkt.flags & AV_PKT_FLAG_KEY);
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
            // video_queues_[st->video_stream_idx_]->push(video_frame);
            // last_pts_list_[video_queues_stream_indices_[st->video_stream_idx_]] = video_frame.timestamp;
        }
    }
    
    if (got_frame != 1) {
        err_again_ = true;  // !HACK: abuse err_again
    }

    av_packet_unref(&pkt);
    return ret;
}

bool Reader::read() {
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

int32_t Reader::framePtsToIndex(int64_t _timestamp) {
    // todo: which stream
    size_t sidx = 0;
    auto * stream = input_streams_[sidx]->stream();
    auto * dec_ctx = input_streams_[sidx]->codec_ctx();
    auto const & fps = input_streams_[sidx]->config().video.fps;

    auto inv_fps = AVRational{fps.den, fps.num};
    auto frame_idx = av_rescale_q(_timestamp - stream->start_time, stream->time_base, inv_fps);
    // spdlog::info("ts {}, tbs {}, inv_fps {}, fps {}", _timestamp, stream->time_base, inv_fps, dec_ctx->framerate);
    return frame_idx;
}

int32_t Reader::timestampToFrameIndex(int64_t _timestamp) {
    // todo: which stream
    size_t sidx = 0;
    auto * stream = input_streams_[sidx]->stream();
    auto * dec_ctx = input_streams_[sidx]->codec_ctx();
    auto const & fps = input_streams_[sidx]->config().video.fps;

    auto inv_fps = AVRational{fps.den, fps.num};
    auto frame_idx = av_rescale_q(_timestamp - stream->start_time, stream->time_base, inv_fps);
    return frame_idx;
}

int64_t Reader::frameIndexToTimestamp(int32_t _frame_idx) {
    // todo: which stream
    size_t sidx = 0;
    auto * stream = input_streams_[sidx]->stream();
    auto const & fps = input_streams_[sidx]->config().video.fps;

    auto inv_fps = AVRational{fps.den, fps.num};
    auto timestamp = av_rescale_q(_frame_idx, inv_fps, stream->time_base) + stream->start_time;
    return timestamp;
}

bool Reader::seek(int32_t _frame_idx) {
    // todo: which stream
    size_t sidx = 0;
    auto * handle = fmt_ctx_.get();
    auto * stream = input_streams_[sidx]->stream();
    auto * dec_ctx = input_streams_[sidx]->codec_ctx();

    // get fps
    auto timestamp = frameIndexToTimestamp(_frame_idx);
    spdlog::debug("seek frame: {}, ts: {}, {}", _frame_idx, timestamp, timestampToFrameIndex(timestamp));

    int ret = av_seek_frame(handle, sidx, timestamp, AVSEEK_FLAG_BACKWARD);
    spdlog::debug("seek? {}", ret);

    return false;
}

int main(int argc, char *argv[]) {
    spdlog::set_level(spdlog::level::debug);

    Reader reader;
    Config cfg;
    cfg.video.resolution = {320, 180};
    if (!reader.open(argv[1], cfg)) {
        spdlog::error("Failed to open video!");
    }

    bool got = false;

    // // for (int i = 0; i < 10; ++i) {
    // int i = 0;
    // while (true) {
    //     {
    //         Timeit _("read");
    //         got = reader.read();
    //     }
    //     if (!got) {
    //         break;
    //     }
    //     SaveFrame(
    //         reader.frame_,
    //         reader.frame_->width,
    //         reader.frame_->height,
    //         i
    //     );
    //     i += 1;
    // }

    int32_t tar = 300;
    reader.seek(tar);
    int32_t cur = -1;
    do {
        got = reader.read();
        cur = reader.framePtsToIndex(reader.frame_->pts);
        spdlog::info("Got frame at {}, {}", reader.frame_->pts, cur);
    } while(cur < tar);

    // for (int i = 0; i < 10; ++i) {
    //     {
    //         // Timeit _("read");
    //         got = reader.read();
    //         spdlog::info("Got frame at {}", reader.frame_->pts);
    //     }
    // }

    return 0;
}
