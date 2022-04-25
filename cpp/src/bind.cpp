#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include "video_reader.hpp"

namespace nb = nanobind;

void naive_demo_bind(nanobind::module_ & m) {
    m.def("test_char_ptr", [](nb::str & str) {
        printf("%s\n", str.c_str());
    });
}

NB_MODULE(ffms, m) {
    naive_demo_bind(m);

    nb::class_<VideoReader>(m, "VideoReader")
        .def(nb::init<std::string>())
        .def(nb::init<std::string, std::string>())
        .def_property_readonly("n_frames", &VideoReader::numFrames)
        .def("read", &VideoReader::read)
    ;
}
