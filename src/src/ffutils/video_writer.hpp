#pragma once
#include "common.hpp"
#include "stream.hpp"
#include <assert.h>
#include <string>
#include <vector>
#include <memory>

namespace ffutils {

class Writer;

class OutputStream : public Stream {
    friend class Writer;
    int64_t next_pts_;
    int32_t sample_count_;
    int32_t frame_capacity_;
public:
    OutputStream()
        : Stream()
        , next_pts_(0), sample_count_(0)
        , frame_capacity_(0) {}
    ~OutputStream() { reset(); }

    void reset() override {
        Stream::reset();
        next_pts_ = 0;
        sample_count_ = 0;
        frame_capacity_ = 0;
    }

    static std::unique_ptr<OutputStream> ConfigureStream(AVFormatContext *oc, enum AVCodecID codecId, const MediaConfig &cfg);
};

class Writer {

};

}
