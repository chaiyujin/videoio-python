#pragma once
#include <ffms.h>
#include <videosource.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/unique_ptr.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/tensor.h>

class VideoReader {
    using NpImage = nanobind::tensor<nanobind::numpy, uint8_t>;
    // * members
    bool is_open_;
    FFMS_ErrorInfo errinfo_;
    FFMS_VideoSource * vid_src_;  // ! Only one video track for now
    const FFMS_VideoProperties * vid_props_;
    int cur_idx_;  // current frame index

    // * hidden ctors
    VideoReader() : is_open_(false), errinfo_(), vid_src_(nullptr), vid_props_(nullptr), cur_idx_(0) {}
    VideoReader(VideoReader const &) = delete;

    // * hidden functions
    bool _openFile(std::string _filepath, std::string _pix_fmt);
    void _cleanup() {
        cur_idx_ = 0;
        if (vid_src_) {
            FFMS_DestroyVideoSource(vid_src_);
            vid_src_ = nullptr;
            vid_props_ = nullptr;
        }
        is_open_ = false;
    }

public:
    VideoReader(std::string _filepath, std::string _pix_fmt = "bgr") : VideoReader() {
        this->_openFile(_filepath, _pix_fmt);
    }
    ~VideoReader() {
        this->_cleanup();
    }

    int numFrames() { return (vid_props_) ? vid_props_->NumFrames : 0; }
    void seekFrame(int _frame_index);
    void seekTime(double _time);

    std::pair<bool, NpImage> read();
};
