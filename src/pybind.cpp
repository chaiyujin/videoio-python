#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <map>
#include "video_reader.hpp"
#include "video_writer.hpp"

namespace py = pybind11;
using namespace pybind11::literals;

using NpImage = py::array_t<uint8_t, py::array::c_style>;
using NpBytes = py::array_t<uint8_t, py::array::c_style>;

auto _CheckInputPixFmt(std::string pix_fmt) -> std::string {
    if      (pix_fmt == "rgb24" || pix_fmt == "bgr24") { return pix_fmt; }
    else if (pix_fmt == "rgba"  || pix_fmt == "bgra" ) { return pix_fmt; }
    else if (pix_fmt == "rgb"   || pix_fmt == "bgr"  ) { return pix_fmt + "24"; }
    else { spdlog::error("[videoio,pybind] Input pix_fmt is unknown: '{}'!", pix_fmt); }
    return "";
}

auto _Read(vio::VideoReader & reader) -> std::pair<bool, NpImage> {
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

bool _OpenReaderWithFile(
    vio::VideoReader & reader,
    std::string filename,
    std::string pix_fmt,
    std::pair<int, int> image_size
) {
    pix_fmt = _CheckInputPixFmt(pix_fmt);
    if (pix_fmt.length() == 0) return false;
    return reader.open(filename, pix_fmt, image_size);
}

bool _OpenReaderWithBytes(
    vio::VideoReader & reader,
    NpBytes const & bytes,
    std::string pix_fmt,
    std::pair<int, int> image_size
) {
    pix_fmt = _CheckInputPixFmt(pix_fmt);
    if (pix_fmt.length() == 0) return false;
    return reader.open(bytes.data(), bytes.size(), pix_fmt, image_size);
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
    pix_fmt = _CheckInputPixFmt(pix_fmt);
    if (pix_fmt.length() == 0) return false;

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
    if (!self.isOpened()) {
        spdlog::error("[videoio,pybind][VideoWriter] Not opened!");
        return false;
    }

    uint8_t const * data = nullptr;
    uint32_t linesize = 0;
    uint32_t height = 0;
    auto w = self.video_config().width;
    auto h = self.video_config().height;
    auto const & pix_fmt = self.video_config().pix_fmt;
    int32_t n = 0;
    if      (pix_fmt == "rgb24" || pix_fmt == "bgr24") { n = 3; }
    else if (pix_fmt == "rgba"  || pix_fmt == "bgra" ) { n = 4; }
    else {
        spdlog::error("[pybind][VideoWriter] VideoWriter use an unknown pix_fmt: '{}'!", pix_fmt);
        return false;
    }
    // Only write in valid case.
    if (
        image.size() != 0 && image.ndim() == 3 &&
        h == image.shape(0) &&
        w == image.shape(1) &&
        n == image.shape(2)
    ) {
        data = image.data();        
        height = image.shape(0);
        linesize = image.strides(0);
        // spdlog::warn("linesize: {}, height: {}, width: {}", linesize, height, image.shape(1));
        self.write(data, linesize, height);
        return true;
    }
    else {
        std::string shape;
        for (int i = 0; i < image.ndim(); ++i) {
            if (i > 0) shape += ",";
            shape += std::to_string(image.shape(i));
        }
        spdlog::warn("[pybind][VideoWriter] Given image has invalid shape ({}), should be ({},{},{})!", shape, h, w, n);
        return false;
    }
}

static std::map<std::string, int> g_str2level = {
    {"quiet",   AV_LOG_QUIET},
    {"panic",   AV_LOG_PANIC},
    {"fatal",   AV_LOG_FATAL},
    {"error",   AV_LOG_ERROR},
    {"warn",    AV_LOG_WARNING}, {"warning", AV_LOG_WARNING},
    {"info",    AV_LOG_INFO},
    {"verbose", AV_LOG_VERBOSE},
    {"debug",   AV_LOG_DEBUG},
    {"trace",   AV_LOG_TRACE},
};

void SetLogLevel(std::string level) {
    auto it = g_str2level.find(level);
    if (it != g_str2level.end()) {
        av_log_set_level(it->second);
    }
}

PYBIND11_MODULE(videoio, m) {
    // Default log level as error.
    av_log_set_level(AV_LOG_ERROR);

    m.def("set_log_level", &SetLogLevel);

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
        .def("open", &_OpenReaderWithFile, "filename"_a, "pix_fmt"_a="bgr24", "image_size"_a=std::pair<int, int>(0, 0))
        .def("open_bytes", &_OpenReaderWithBytes, "bytes"_a, "pix_fmt"_a="bgr24", "image_size"_a=std::pair<int, int>(0, 0))
        .def("seek_frame", &vio::VideoReader::seekByFrame)
        .def("seek_msec", [](vio::VideoReader & r, float msec) -> bool { return r.seekByTime(vio::Millisecond((int64_t)std::round(msec))); })
        .def("release", &vio::VideoReader::close)
        .def("close", &vio::VideoReader::close)
        .def("read", &_Read, py::return_value_policy::move)
        // static
        .def_static("set_log_level", &SetLogLevel)
    ;

    py::class_<vio::VideoWriter>(m, "VideoWriter")
        .def(py::init<>())
        .def("open", &_OpenWriter, "filename"_a, "image_size"_a, "fps"_a, "pix_fmt"_a="bgr24", "bitrate"_a=0, "crf"_a=23.0, "g"_a=12)
        .def("release", &vio::VideoWriter::close)
        .def("close", &vio::VideoWriter::close)
        .def("write", &_Write)
        // static
        .def_static("set_log_level", &SetLogLevel)
    ;
}
