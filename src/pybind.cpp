#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include "video_reader.hpp"
#include "video_writer.hpp"

namespace py = pybind11;
using namespace pybind11::literals;

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

bool _OpenWriter(
    vio::VideoWriter & self,
    std::string filename,
    std::pair<int32_t, int32_t> image_size,
    double fps,
    std::string pix_fmt,
    int32_t bitrate,
    double crf,
    int32_t g
) {
    vio::VideoConfig cfg;
    cfg.width = image_size.first;
    cfg.height = image_size.second;
    cfg.fps = av_d2q(fps, 1000000);
    cfg.pix_fmt = pix_fmt;
    cfg.bitrate = bitrate;
    cfg.crf = crf;
    cfg.g = g;
    return self.open(filename, cfg);
}

bool _Write(vio::VideoWriter & self, NpImage const & image) {
    uint8_t const * data = nullptr;
    uint32_t linesize = 0;
    uint32_t height = 0;
    if ((image.size() != 0) && (image.ndim() == 3)) {
        data = image.data();        
        height = image.shape(0);
        linesize = image.strides(0);
        spdlog::warn("linesize: {}, height: {}, width: {}", linesize, height, image.shape(1));
    }
    return self.write(data, linesize, height);
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
        // We return tbr rather than fps here.
        .def_property_readonly("fps", [](vio::VideoReader const & r) { auto tbr = r.tbr(); return (double)tbr.num / (double)tbr.den; })
        .def("open", &vio::VideoReader::open, "filename"_a, "image_size"_a=std::pair<int, int>(0, 0))
        .def("seek_frame", &vio::VideoReader::seekByFrame)
        .def("seek_msec", &vio::VideoReader::seekByTime)
        .def("release", &vio::VideoReader::close)
        .def("close", &vio::VideoReader::close)
        .def("read", &_Read, py::return_value_policy::move)
    ;

    py::class_<vio::VideoWriter>(m, "VideoWriter")
        .def(py::init<>())
        .def("open", &_OpenWriter, "filename"_a, "image_size"_a, "fps"_a, "pix_fmt"_a="bgr24", "bitrate"_a=0, "crf"_a=23.0, "g"_a=12)
        .def("release", &vio::VideoWriter::close)
        .def("close", &vio::VideoWriter::close)
        .def("write", &_Write)
    ;
}
