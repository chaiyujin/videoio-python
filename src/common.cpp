#include "common.hpp"
#include "log.hpp"

namespace vio {

#if LIBAVUTIL_VERSION_MAJOR >= 57
AVFrame *AllocateFrame(
    enum AVSampleFormat     sampleFmt,
    AVChannelLayout const & channelLayout,
    int                     sampleRate,
    int                     nbSamples
) {
    AVFrame *frame = av_frame_alloc();
    if (!frame) { spdlog::critical("[ffmpeg] Error allocating an audio fram"); exit(1); }
    frame->format      = sampleFmt;
    frame->ch_layout   = channelLayout;
    frame->sample_rate = sampleRate;
    frame->nb_samples  = nbSamples;
    frame->pts         = AV_NOPTS_VALUE;
    if (nbSamples) {
        int ret = av_frame_get_buffer(frame, 0);
        if (ret < 0) { spdlog::critical("[ffmpeg] Error allocating an audio buffer"); exit(1); }
    }
    return frame;
}
#else
AVFrame *AllocateFrame(
    enum AVSampleFormat sampleFmt,
    uint64_t            channelLayout,
    int                 sampleRate,
    int                 nbSamples
) {
    AVFrame *frame = av_frame_alloc();
    if (!frame) { spdlog::critical("[ffmpeg] Error allocating an audio fram"); exit(1); }
    frame->format         = sampleFmt;
    frame->channel_layout = channelLayout;
    frame->sample_rate    = sampleRate;
    frame->nb_samples     = nbSamples;
    frame->pts            = AV_NOPTS_VALUE;
    if (nbSamples) {
        int ret = av_frame_get_buffer(frame, 0);
        if (ret < 0) { spdlog::critical("[ffmpeg] Error allocating an audio buffer"); exit(1); }
    }
    return frame;
}
#endif

AVFrame * AllocateFrame(enum AVPixelFormat pix_fmt, int width, int height) {
    AVFrame * picture = av_frame_alloc();
    if (!picture) return nullptr;

    picture->format = pix_fmt;
    picture->width  = width;
    picture->height = height;
    picture->pts    = AV_NOPTS_VALUE;

    /* allocate the buffers for the frame data */
    int ret = av_frame_get_buffer(picture, 32);
    if (ret < 0) { spdlog::critical("[ffmpeg] Could not allocate frame data: {}", av_err2str(ret)); exit(1); }

    return picture;
}

}