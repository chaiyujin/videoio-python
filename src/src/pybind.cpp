#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include "ffutils/ffutils.hpp"

namespace py = pybind11;

using NpImage = py::array_t<uint8_t, py::array::c_style>;

std::pair<bool, NpImage> _Read(ffutils::VideoReader & _reader) {

    static size_t shape_empty[3] = { 0, 0, 0 };
    static NpImage empty(shape_empty);

    bool got = _reader.read();
    if (!got) {
        return {false, empty};
    }

    AVFrame * frame = _reader.frame();

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

PYBIND11_MODULE(ffutils, m) {
    py::class_<ffutils::VideoReader>(m, "VideoReader")
        .def(py::init<std::string>())
        .def(py::init<std::string, std::string>())
        .def_property_readonly("n_frames", &ffutils::VideoReader::n_frames)
        .def_property_readonly("duration", [](ffutils::VideoReader const & r) {
            auto ts = r.duration();
            return cast<MsDouble>(ts).count();
        })
        .def_property_readonly("curr_msec", [](ffutils::VideoReader const & r) {
            auto ts = r.current_timestamp();
            return cast<MsDouble>(ts).count();
        })
        .def_property_readonly("resolution", [](ffutils::VideoReader const & r) {
            auto res = r.resolution();
            return std::pair<int, int>(res.x, res.y);
        })
        .def_property_readonly("width", [](ffutils::VideoReader const & r) {
            return r.resolution().x;
        })
        .def_property_readonly("height", [](ffutils::VideoReader const & r) {
            return r.resolution().y;
        })
        .def_property_readonly("fps", [](ffutils::VideoReader const & r) {
            auto fps = r.fps();
            return (double)fps.num / (double)fps.den;
        })
        .def("seek_frame", &ffutils::VideoReader::seek)
        .def("seek_msec", &ffutils::VideoReader::seekTime)
        .def("read", &_Read, py::return_value_policy::move)
        .def("release", &ffutils::VideoReader::close)
        .def("close", &ffutils::VideoReader::close)
    ;
}
