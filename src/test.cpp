#include "log.hpp"
#include "video_reader.hpp"
#include "video_writer.hpp"
using namespace vio;

int main(int argc, char ** argv) {
    // if (argc != 2) {
    //     spdlog::error("Wrong number of arguments! {}", argc);
    // }

    // VideoReader reader;
    // spdlog::info("Try to open: {}, {}", argv[1], reader.open(argv[1]));

    // reader.seekByTime(Millisecond(100));
    // reader.seekByFrame(880);
    // reader.read();
    // spdlog::info("Current frame: {}, time: {}, fidx: {}",
    //     reader._ts_to_fidx(reader.frame_->pts),
    //     reader.currMillisecond(),
    //     reader.currFrameIndex()
    // );

    // int count = 0;
    // while (reader._getFrame()) {
    //     auto * frame = reader.frame_;
    //     spdlog::info("read: {}, {}/{}", frame->pts, ++count, reader.numFrames());
    // }
    // avcodec_flush_buffers(reader.main_stream_data_->codec_ctx());
    // av_seek_frame(reader.fmtctx_.get(), -1, 0, AVSEEK_FLAG_BACKWARD);

    // spdlog::warn("try to read after end and seek back:");
    // spdlog::warn("{}, {}", reader._getFrame(), reader.frame_->pts);


    {
        spdlog::set_level(spdlog::level::debug);
        // allocate the output media context
        std::string filename_ = "test.mp4";
        VideoWriter writer;
        VideoConfig cfg;
        cfg.width = 640; cfg.height = 480;
        cfg.pix_fmt = "bgr24";
        cfg.fps = {30, 1};
        cfg.crf = 20.0;
        cfg.g = 0;

        std::vector<uint8_t> data(640*480*3, 120);

        writer.open(filename_, cfg);
        spdlog::info("opened: {}", writer.isOpened());
        for (int i = 0; i < 100; ++i) {
            printf("-----------------\n");
            spdlog::info("write: {}", writer.write(data.data(), 640*3, 480));
        }
        writer.close();
    }

    return 0;
}
