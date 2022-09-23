#include "log.hpp"
#include "video_reader.hpp"
using namespace vio;

int main(int argc, char ** argv) {
    if (argc != 2) {
        spdlog::error("Wrong number of arguments! {}", argc);
    }

    VideoReader reader;
    spdlog::info("Try to open: {}, {}", argv[1], reader.open(argv[1]));

    reader.seekByTime(Millisecond(100));
    reader.seekByFrame(880);
    reader.read();
    spdlog::info("Current frame: {}, time: {}, fidx: {}",
        reader._ts_to_fidx(reader.frame_->pts),
        reader.currMillisecond(),
        reader.currFrameIndex()
    );

    // int count = 0;
    // while (reader._getFrame()) {
    //     auto * frame = reader.frame_;
    //     spdlog::info("read: {}, {}/{}", frame->pts, ++count, reader.numFrames());
    // }
    // avcodec_flush_buffers(reader.main_stream_data_->codec_ctx());
    // av_seek_frame(reader.fmtctx_.get(), -1, 0, AVSEEK_FLAG_BACKWARD);

    // spdlog::warn("try to read after end and seek back:");
    // spdlog::warn("{}, {}", reader._getFrame(), reader.frame_->pts);

    return 0;
}
