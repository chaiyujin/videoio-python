#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/pair.h>
#include <nanobind/tensor.h>
#include "ffutils/ffutils.hpp"

namespace nb = nanobind;

using NpImage = nb::tensor<nanobind::numpy, uint8_t>;

std::pair<bool, NpImage> _Read(ffutils::VideoReader & _reader) {

    // spdlog::set_level(spdlog::level::debug);

    static size_t shape_empty[3] = { 0, 0, 0 };
    static NpImage empty(nullptr, 3, shape_empty);

    bool got = _reader.read();
    if (!got) {
        return {false, empty};
    }

    AVFrame * frame = _reader.frame();

    // get data
    auto const h = frame->height;
    auto const w = frame->width;
    auto const s = frame->linesize[0];
    uint8_t * data = new uint8_t[h * s];
    memcpy(data, frame->data[0], h * s);
    // printf("%d %d %d\n", h, w, s);

    // Delete 'data' when the 'owner' capsule expires
    nb::capsule owner(data, [](void *p) {
       delete[] (uint8_t *) p;
    });

    int chs = s / w;  // TODO: channels
    size_t shape[3] = { (size_t)h, (size_t)w, (size_t)chs };
    int64_t strides[3] = { (int64_t)s, (int64_t)chs, (int64_t)1 };
    return {
        true,
        NpImage(data, 3, shape, owner, strides)
    };
}

NB_MODULE(ffutils, m) {
    nb::class_<ffutils::VideoReader>(m, "VideoReader")
        .def(nb::init<std::string>())
        .def(nb::init<std::string, std::string>())
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
            auto res = r.resolution();
            return res.x;
        })
        .def_property_readonly("height", [](ffutils::VideoReader const & r) {
            auto res = r.resolution();
            return res.y;
        })
        .def("seek_frame", &ffutils::VideoReader::seek)
        .def("seek_msec", &ffutils::VideoReader::seekTime)
        .def("read", &_Read)
        .def("release", &ffutils::VideoReader::close)
        .def("close", &ffutils::VideoReader::close)
    ;
}
