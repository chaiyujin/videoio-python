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
        this->close();
    }

    auto open(std::string const & filename, VideoConfig cfg) -> bool;
    void close();
    auto isOpened() const -> bool { return video_stream_data_ != nullptr; }

    auto write(const uint8_t * data, uint32_t linesize, uint32_t height) -> bool {
        return _writeVideoFrame(data, linesize, height);
    }

private:
    std::unique_ptr<AVFormatContext, void(*)(AVFormatContext *)> fmtctx_;
    std::unique_ptr<OutputStreamData> video_stream_data_;

    auto _writeVideoFrame(const uint8_t * data, uint32_t linesize, uint32_t height) -> bool;

    void _cleanup() {
        video_stream_data_.reset();
        fmtctx_.reset();
    }
};

}
