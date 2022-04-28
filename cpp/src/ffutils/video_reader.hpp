#pragma once

#include "common.hpp"
#include "stream.hpp"
#include <string>
#include <memory>

namespace ffutils {

#define AV_NOIDX_VALUE ((int32_t)UINT32_C(0x80000000))

class VideoReader;

inline void AVFormatContextDeleter(AVFormatContext *ctx) {
    if (ctx) {
        spdlog::debug("avformat_close_input");
        avformat_close_input(&ctx);
    }
}

class CircleBuffer {
    size_t head_;
    size_t tail_;
    size_t size_;
    std::vector<AVFrame *> buffer_;
    bool allocated_;

    void _inc(size_t & _idx) {
        _idx = (_idx + 1) % buffer_.size();
    }

    void _cleanup() {
        for (auto * & p : buffer_) {
            if (p) {
                av_frame_free(&p);
                p = nullptr;
            }
        }
        buffer_.clear();
        allocated_ = false;
        head_ = tail_ = 0;
    }

public:
    CircleBuffer()
        : head_(0), tail_(0)  // [head_, tail_)
        , size_(0), buffer_()
        , allocated_(false)
        // ! one extra item for judging full queue.
        // ! so that, full is (tail_ + 1 == head_), empty is (tail_ == head_)
    {}
    ~CircleBuffer() {
        this->_cleanup();
    }

    void clear() {
        this->_cleanup();
    }

    void allocate(size_t size, enum AVPixelFormat pixFmt, int width, int height) {
        this->_cleanup();
        buffer_.resize(size + 1, nullptr);
        size_ = size;
        for (size_t i = 0; i < buffer_.size(); ++i) {
            buffer_[i] = AllocateFrame(pixFmt, width, height);
        }
        allocated_ = true;
    }

    size_t size() const {
        return size_;
    }

    size_t n_elements() const {
        if (tail_ >= head_) {
            return tail_ - head_;
        } else {
            return tail_ + buffer_.size() - head_;
        }
    }

    bool is_empty() const {
        return tail_ == head_;
    }

    bool is_full() const {
        return (tail_ + 1 == head_) || (head_ == 0 && tail_ == size_);
    }

    void push_back() {
        this->_inc(tail_);
        if (tail_ == head_) {
            this->_inc(head_);
        }
    }

    void pop_front() {
        // ! must check not empty before pop;
        if (!this->is_empty()) {
            this->_inc(head_);
        }
    }

    AVFrame * offset_front(size_t _off) {
        assert(allocated_);
        return buffer_[(head_ + _off) % buffer_.size()];
    }

    AVFrame * offset_back(size_t _off) {
        assert(allocated_);
        _off += 1; // since 'tail_-1' point to last element
        size_t t = tail_;
        if (t < _off) {
            t += buffer_.size();
        }
        return buffer_[t - _off];
    }
};

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

    // count frames
    int32_t n_frames_;
    // frame buffer
    CircleBuffer frame_buffer_;  // cache buffer

public:

    InputStream()
        : Stream()
        , start_ts_(AV_NOPTS_VALUE)
        , next_dts_(AV_NOPTS_VALUE)
        , curr_dts_(AV_NOPTS_VALUE)
        , next_pts_(AV_NOPTS_VALUE)
        , curr_pts_(AV_NOPTS_VALUE)
        , video_stream_idx_(-1)
        , audio_stream_idx_(-1)
        , n_frames_(0)
        , frame_buffer_()
    {}
    ~InputStream() {
        this->reset();
    }

    void reset() override {
        Stream::reset();
        start_ts_ = AV_NOPTS_VALUE;
        next_dts_ = curr_dts_ = AV_NOPTS_VALUE;
        next_pts_ = curr_pts_ = AV_NOPTS_VALUE;
        video_stream_idx_ = audio_stream_idx_ = -1;
        n_frames_ = 0;
        frame_buffer_.clear();
    }

    CircleBuffer & buffer() {
        return frame_buffer_;
    }
};


class VideoReader {
    bool is_open_;
    bool is_eof_;
    int32_t read_idx_;
    // for decoding
    bool err_again_;
    AVFrame * frame_;  // point to last frame
    // ffmpeg related
    std::unique_ptr<AVFormatContext, void(*)(AVFormatContext *ctx)> fmt_ctx_;
    std::vector<std::unique_ptr<InputStream>> input_streams_;
    std::vector<InputStream *> video_streams_;

    void _cleanup() {
        is_open_ = false;
        is_eof_ = false;
        read_idx_ = -1;
        // decoding
        err_again_ = false;
        frame_ = nullptr;  // handled by creators (stream)
        // ffmpeg related, must be cleaned in correct order.
        input_streams_.clear();
        video_streams_.clear();
        fmt_ctx_.reset(nullptr);
    }

    int _process_packet();
    void _convert_pix_fmt();
    int32_t _ts_to_fidx(int64_t);
    int64_t _fidx_to_ts(int32_t);
    bool _read_frame();
public:

    VideoReader()
        : is_open_(false)
        , is_eof_(false)
        , read_idx_(-1)
        , err_again_(false)
        , frame_(nullptr)
        , fmt_ctx_(nullptr, AVFormatContextDeleter)
        , input_streams_()
        , video_streams_() {
        _cleanup();
    }
    VideoReader(std::string _filepath, std::string _pix_fmt = "bgr") : VideoReader() {
        MediaConfig media_config;
        AVPixelFormat pix_fmt;
        if      (_pix_fmt == "rgb")  { pix_fmt = AV_PIX_FMT_RGB24; }
        else if (_pix_fmt == "bgr")  { pix_fmt = AV_PIX_FMT_BGR24; }
        else if (_pix_fmt == "rgba") { pix_fmt = AV_PIX_FMT_RGBA; }
        else if (_pix_fmt == "bgra") { pix_fmt = AV_PIX_FMT_BGRA; }
        else {
            pix_fmt = AV_PIX_FMT_BGR24;
            spdlog::warn(
                "Unknown pixel format '{}', use 'bgr'. "
                "Or you can choose one of ['rgb', 'bgr', 'rgba', 'bgra']",
                _pix_fmt
            );
        }

        media_config.video.pix_fmt = pix_fmt;
        this->open(_filepath, media_config);
    }
    ~VideoReader() {
        _cleanup();
    }

    bool is_open() const { return is_open_; }
    bool is_eof()  const { return is_eof_ || (read_idx_ + 1 >= n_frames()); }
    int32_t n_frames() const { return (is_open_) ? video_streams_[0]->n_frames_ : 0; }
    Timestamp current_timestamp() const {
        return (is_open_)
            ? ((frame_) ? AVTimeToTimestamp(frame_->pts, video_streams_[0]->stream()->time_base) : Timestamp(0))
            : Timestamp(AV_NOPTS_VALUE);
    }

    // only care about the first video track
    auto * frame() { return frame_; }
    bool open(std::string _filepath, MediaConfig _cfg = MediaConfig());
    bool seek(int32_t _frame_idx);
    bool read();
};

}
