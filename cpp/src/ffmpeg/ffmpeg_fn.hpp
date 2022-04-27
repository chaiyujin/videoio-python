#pragma once
#include "_common.hpp"
#include <vector>


int Decode(AVCodecContext *avctx, AVFrame  *frame, int *gotFrame, AVPacket *pkt);
int Encode(AVCodecContext *avctx, AVPacket *pkt,   int *gotPacket, AVFrame *frame);
AVFrame * AllocateFrame(enum AVSampleFormat sampleFmt, uint64_t channelLayout, int sampleRate, int nbSamples);
AVFrame * AllocateFrame(enum AVPixelFormat pixFmt, int width, int height);
std::vector<float> Resample(const std::vector<float> &audio, int srcSampleRate, int dstSampleRate);
void _SetLogLevel();

inline Timestamp AVTimeToTimestamp(int64_t _av_time, AVRational _time_base = AV_TIME_BASE_Q)
{
    return Timestamp(av_rescale_q(_av_time, _time_base, {1, 1000000}));
}
inline int64_t TimestampToAVTime(Timestamp _ts, AVRational _time_base = AV_TIME_BASE_Q)
{
    return av_rescale_q(_ts.count(), {1, 1000000}, _time_base);
}
