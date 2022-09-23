#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include "video_reader.hpp"

namespace py = pybind11;

using NpImage = py::array_t<uint8_t, py::array::c_style>;

std::pair<bool, NpImage> _Read(vio::VideoReader & reader) {

    static size_t shape_empty[3] = { 0, 0, 0 };
    static NpImage empty(shape_empty);

    bool got = reader.read();
    if (!got) {
        return {false, empty};
    }

    const AVFrame * frame = reader.frame();

    // get data
    auto const h = frame->height;
    auto const w = frame->width;
    auto const s = frame->linesize[0];
    // snow::log::warn("h {}, w {}, s{}\n", h, w, s);

    int chs = s / w;  // TODO: channels
    size_t shape[3] = { (size_t)h, (size_t)w, (size_t)chs };
    NpImage ret(shape);
    for (int y = 0; y < h; ++y) {
        memcpy(ret.mutable_data() + (w * chs * y), frame->data[0] + s * y, w * chs);
    }
    return {true, std::move(ret)};
}

PYBIND11_MODULE(videoio, m) {
    py::class_<vio::VideoReader>(m, "VideoReader")
        .def(py::init<>())
        .def_property_readonly("n_frames", &vio::VideoReader::numFrames)
        .def_property_readonly("duration", [](vio::VideoReader const & r) { auto ts = r.duration(); return vio::cast<vio::MsDouble>(ts).count(); })
        .def_property_readonly("curr_msec", [](vio::VideoReader const & r) { auto ts = r.currMillisecond(); return vio::cast<vio::MsDouble>(ts).count(); })
        .def_property_readonly("curr_iframe", [](vio::VideoReader const & r) { return r.currFrameIndex(); })
        .def_property_readonly("image_size", [](vio::VideoReader const & r) { return r.imageSize(); })
        .def_property_readonly("width", [](vio::VideoReader const & r) { return r.imageSize().first; })
        .def_property_readonly("height", [](vio::VideoReader const & r) { return r.imageSize().second; })
        .def_property_readonly("fps", [](vio::VideoReader const & r) { auto fps = r.fps(); return (double)fps.num / (double)fps.den; })
        .def("open", &vio::VideoReader::open, py::arg("filename"), py::arg("image_size")=std::pair<int, int>(0, 0))
        .def("seek_frame", &vio::VideoReader::seekByFrame)
        .def("seek_msec", &vio::VideoReader::seekByTime)
        .def("release", &vio::VideoReader::close)
        .def("close", &vio::VideoReader::close)
        .def("read", &_Read, py::return_value_policy::move)
    ;
}
