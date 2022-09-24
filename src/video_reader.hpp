#pragma once
#include <string>
#include <memory>
#include "avio.hpp"
#include "stream.hpp"

namespace vio {

class VideoReader {
public:
    VideoReader()
        : ioctx_(nullptr)
        , fmtctx_(nullptr, [](AVFormatContext * p) { avformat_free_context(p); })
        , main_stream_idx_(0)
        , main_stream_data_(nullptr)
        , start_time_(kNoTimestamp), duration_(kNoTimestamp)
        , fps_({1, 0}), tbr_({1, 0})
        , frame_(nullptr)
        , read_idx_(-1)
        , seek_to_pts_(true)
        , dts_pts_delta_(0)
    {}
    ~VideoReader() {
        this->close();
    }

    bool open(std::string const & filename, std::pair<int32_t, int32_t> const & target_resolution = {0, 0});
    bool isOpened() const { return main_stream_data_ != nullptr; }
    void close();

    auto fps() const -> AVRational { return fps_; }
    auto tbr() const -> AVRational { return tbr_; }
    auto duration() const -> Millisecond const { return (isOpened()) ? duration_ : Millisecond(0); }
    auto numFrames() const -> uint64_t {
        if (isOpened()) {
            uint64_t ret = main_stream_data_->stream()->nb_frames;
            if (ret == 0) ret = (uint64_t)((double)duration_.count() / 1000.0 * fps_.num / fps_.den);
            return ret;
        }
        return 0;
    }
    auto imageSize() const -> std::pair<int, int> { return (isOpened()) ? main_stream_data_->image_size() : std::pair<int, int>(0, 0);}
    Millisecond currMillisecond() const {
        return (isOpened())
            ? ((frame_) ? AVTime2MS(frame_->pts, main_stream_data_->stream()->time_base) : Millisecond(0))
            : kNoTimestamp;
    }
    int32_t currFrameIndex() const {
        return (isOpened())
            ? ((frame_) ? _ts_to_fidx(frame_->pts) : 0)
            : -1;
    }

    auto read() -> bool;
    auto seekByFrame(int32_t) -> bool;
    auto seekByTime(Millisecond ms) -> bool;
    auto frame() const -> const AVFrame * { return frame_; }

private:
public:
    // The file IO and format context
    std::unique_ptr<AVIOBase> ioctx_;
    std::unique_ptr<AVFormatContext, void(*)(AVFormatContext *)> fmtctx_;
    // The main stream
    size_t main_stream_idx_;
    std::unique_ptr<InputStreamData> main_stream_data_;

    // some properties
    Millisecond start_time_;
    Millisecond duration_;
    AVRational fps_;
    AVRational tbr_;

    // for decoding, reading and seeking
    AVFrame const * frame_;
    int32_t read_idx_;
    bool seek_to_pts_;
    int64_t dts_pts_delta_;

    auto _findMainStream(std::pair<int32_t, int32_t> const & target_resolution) -> bool;
    auto _getFrame() -> bool;
    auto _readPacket(AVPacket *) -> int;
    void _convertPixFmt();
    int64_t _fidx_to_ts(int32_t) const;
    int32_t _ts_to_fidx(int64_t) const;

    auto _seekToPTS() const -> bool { return seek_to_pts_; }

    void _cleanup() {
        dts_pts_delta_ = 0;
        seek_to_pts_ = true;
        read_idx_ = -1;
        frame_ = nullptr;
        fps_ = tbr_ = {1, 0};
        start_time_ = duration_ = kNoTimestamp;
        main_stream_data_.reset();
        main_stream_idx_ = 0;
        fmtctx_.reset();
        ioctx_.reset();
    }
};

}
