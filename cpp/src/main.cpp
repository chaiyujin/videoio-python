#include "read_video.hpp"

static void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
    FILE *pFile;
    char szFilename[32];
    int  y;
    
    // Open file
    sprintf(szFilename, "hello/frame%d.ppm", iFrame);

    pFile=fopen(szFilename, "wb");
    if(pFile==NULL)
        return;
    
    // Write header
    fprintf(pFile, "P6\n%d %d\n255\n", width, height);
    
    // Write pixel data
    for(y=0; y<height; y++)
        fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);
    
    // Close file
    fclose(pFile);
}

int main(int argc, char *argv[]) {
    spdlog::set_level(spdlog::level::debug);

    ffutils::VideoReader reader;
    ffutils::MediaConfig cfg;
    // cfg.video.resolution = {320, 180};
    if (!reader.open(argv[1], cfg)) {
        spdlog::error("Failed to open video!");
    }

    bool got = false;

    if (reader.is_open()) {
        int32_t tar = 300;
        Timeit _(fmt::format("seek frame {}", tar));
        reader.seek(tar);
        int32_t cur = -1;
        do {
            got = reader.read();
            cur = reader._framePtsToIndex(reader.frame_->pts);
            spdlog::info("Got frame at {}, {}", reader.frame_->pts, cur);
        } while(cur < tar);
    }

    return 0;
}
