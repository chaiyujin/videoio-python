#pragma once
#include <string>
#include <memory>
#include "avio.hpp"
#include "stream.hpp"

namespace vio {

class VideoWriter {
public:
    VideoWriter()
        : fmtctx_(nullptr, [](AVFormatContext * p) { avformat_free_context(p); })
        , video_stream_data_(nullptr)
    {}
    ~VideoWriter() {
        this->_cleanup();
    }

    auto open(std::string const & filename, VideoConfig cfg) -> bool;
    void close();
    auto isOpened() const -> bool { return video_stream_data_ != nullptr; }

private:
    std::unique_ptr<AVFormatContext, void(*)(AVFormatContext *)> fmtctx_;
    std::unique_ptr<OutputStreamData> video_stream_data_;

    auto _writeVideoFrame(const uint8_t * data, int32_t linesize, int32_t height) -> bool;

    void _cleanup() {
        video_stream_data_.reset();
        fmtctx_.reset();
    }
};

}
