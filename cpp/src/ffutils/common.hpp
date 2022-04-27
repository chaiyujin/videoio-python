/**
 * The includer for ffmpeg components
 * Some functions are adapted to 
 * */
#pragma once

extern "C" {

#include <libavutil/opt.h>
#include <libavutil/dict.h>
#include <libavutil/eval.h>
#include <libavutil/fifo.h>
#include <libavutil/time.h>
#include <libavutil/avutil.h>
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/rational.h>
#include <libavutil/timestamp.h>
#include <libavutil/samplefmt.h>
#include <libavutil/hwcontext.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/threadmessage.h>
#include <libavutil/channel_layout.h>
#include <libavformat/avio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

}

#include "../utils/utils.hpp"

inline std::ostream & operator<<(std::ostream & _out, AVRational const & _av_rational) {
    _out << _av_rational.num << "/" << _av_rational.den;
    return _out;
}

namespace ffutils {

    int Decode(AVCodecContext *avctx, AVFrame  *frame, int *gotFrame, AVPacket *pkt);
    int Encode(AVCodecContext *avctx, AVPacket *pkt,   int *gotPacket, AVFrame *frame);

    AVFrame * AllocateFrame(enum AVSampleFormat sampleFmt, uint64_t channelLayout, int sampleRate, int nbSamples);
    AVFrame * AllocateFrame(enum AVPixelFormat pixFmt, int width, int height);

    std::vector<float> Resample(const std::vector<float> &audio, int srcSampleRate, int dstSampleRate);

    inline Timestamp AVTimeToTimestamp(int64_t _av_time, AVRational _time_base = AV_TIME_BASE_Q) {
        return Millisecond(av_rescale_q(_av_time, _time_base, {1, 1000}));
    }
    inline int64_t TimestampToAVTime(Millisecond _ts, AVRational _time_base = AV_TIME_BASE_Q) {
        return av_rescale_q(_ts.count(), {1, 1000}, _time_base);
    }

}
