#pragma once

#include "ffmpeg/ffmpeg.hpp"
#include <string>
#include <memory>

inline std::ostream & operator<<(std::ostream & _out, AVRational const & _av_rational) {
    _out << _av_rational.num << "/" << _av_rational.den;
    return _out;
}

inline void AVFormatContextDeleter(AVFormatContext *ctx) {
    if (ctx) {
        spdlog::debug("avformat_close_input");
        avformat_close_input(&ctx);
    }
}

class Reader;

class InputStream : public Stream
{
    friend class Reader;
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

    void reset() override
    {
        Stream::reset();
        start_ts_ = AV_NOPTS_VALUE;
        next_dts_ = curr_dts_ = AV_NOPTS_VALUE;
        next_pts_ = curr_pts_ = AV_NOPTS_VALUE;
        video_stream_idx_ = audio_stream_idx_ = -1;
    }
};

class Reader {
public:
    bool is_open_;
    // for decoding
    bool err_again_;
    AVFrame * frame_;
    // ffmpeg related
    std::unique_ptr<AVFormatContext, void(*)(AVFormatContext *ctx)> fmt_ctx_;
    std::vector<std::unique_ptr<InputStream>> input_streams_;

    void _cleanup() {
        is_open_ = false;
        // decoding
        err_again_ = false;
        frame_ = nullptr;  // handled by creators (stream)
        // ffmpeg related, must be cleaned in correct order.
        input_streams_.clear();
        fmt_ctx_.reset(nullptr);
    }

    int _processInput();

    Reader()
        : is_open_(false)
        , err_again_(false)
        , frame_(nullptr)
        , fmt_ctx_(nullptr, AVFormatContextDeleter)
        , input_streams_()
    { _cleanup(); }

    bool is_open() const { return is_open_; }

    bool open(std::string _filepath, Config _cfg = Config());
    bool read();
    bool seek(int32_t _frame_idx);

    int32_t timestampToFrameIndex(int64_t);
    int64_t frameIndexToTimestamp(int32_t);
    int32_t framePtsToIndex(int64_t);
};
