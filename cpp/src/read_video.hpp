#pragma once

#include "ffutils/ffutils.hpp"
#include <string>
#include <memory>

namespace ffutils {

class VideoReader;

inline void AVFormatContextDeleter(AVFormatContext *ctx) {
    if (ctx) {
        spdlog::debug("avformat_close_input");
        avformat_close_input(&ctx);
    }
}

class InputStream : public Stream {
    friend class VideoReader;
    // sync timestamps
    int64_t start_ts_;
    int64_t next_dts_;         // predicted dts of the next packet read for this stream or
                               // (when there are several frames in a packet) of the next
                               // frame in current packet (in AV_TIME_BASE units)
    int64_t curr_dts_;         // dts of the last packet read for this stream (in AV_TIME_BASE units)
    int64_t next_pts_;         // synthetic pts for the next decode frame (in AV_TIME_BASE units)
    int64_t curr_pts_;         // current pts of the decoded frame  (in AV_TIME_BASE units)
    int32_t video_stream_idx_; // stream index in all video streams
    int32_t audio_stream_idx_; // stream index in all audio streams

public:

    InputStream()
        : Stream()
        , start_ts_(AV_NOPTS_VALUE)
        , next_dts_(AV_NOPTS_VALUE)
        , curr_dts_(AV_NOPTS_VALUE)
        , next_pts_(AV_NOPTS_VALUE)
        , curr_pts_(AV_NOPTS_VALUE)
        , video_stream_idx_(-1)
        , audio_stream_idx_(-1) {}
    ~InputStream() {}

    void reset() override {
        Stream::reset();
        start_ts_ = AV_NOPTS_VALUE;
        next_dts_ = curr_dts_ = AV_NOPTS_VALUE;
        next_pts_ = curr_pts_ = AV_NOPTS_VALUE;
        video_stream_idx_ = audio_stream_idx_ = -1;
    }
};

class VideoReader {
public:
    bool is_open_;
    // for decoding
    bool err_again_;
    AVFrame * frame_;
    // ffmpeg related
    std::unique_ptr<AVFormatContext, void(*)(AVFormatContext *ctx)> fmt_ctx_;
    std::vector<std::unique_ptr<InputStream>> input_streams_;
    std::vector<InputStream *> video_streams_;

    void _cleanup() {
        is_open_ = false;
        // decoding
        err_again_ = false;
        frame_ = nullptr;  // handled by creators (stream)
        // ffmpeg related, must be cleaned in correct order.
        input_streams_.clear();
        video_streams_.clear();
        fmt_ctx_.reset(nullptr);
    }

    int _processInput();
    int32_t _timestampToFrameIndex(int64_t);
    int64_t _frameIndexToTimestamp(int32_t);
    int32_t _framePtsToIndex(int64_t);

    VideoReader()
        : is_open_(false)
        , err_again_(false)
        , frame_(nullptr)
        , fmt_ctx_(nullptr, AVFormatContextDeleter)
        , input_streams_()
        , video_streams_()
    {
        _cleanup();
    }
    ~VideoReader() {
        _cleanup();
    }

    bool is_open() const { return is_open_; }

    // only care about the first video track
    bool open(std::string _filepath, MediaConfig _cfg = MediaConfig());
    bool read();
    bool seek(int32_t _frame_idx);

};

}
