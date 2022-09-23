#pragma once
#include <chrono>
#include <iostream>

// * -------------------------------------------------------------------------------------------------------------- * //
// *                                               Hijack some macros                                               * //
// * -------------------------------------------------------------------------------------------------------------- * //

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/timestamp.h>
}

#undef  av_ts2str
#undef  av_err2str
#undef  av_ts2timestr
#undef  AV_TIME_BASE_Q
#define AV_TIME_BASE_Q AVRational {1, AV_TIME_BASE}

inline char *av_ts2str(int64_t ts) {
    char str[AV_TS_MAX_STRING_SIZE];
    memset(str, 0, sizeof(str));
    return av_ts_make_string(str, ts);
}

inline char* av_err2str(int errnum) {
    char str[AV_ERROR_MAX_STRING_SIZE];
    memset(str, 0, sizeof(str));
    return av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, errnum);
}

inline char *av_ts2timestr(int64_t ts, AVRational tb) {
    char str[AV_TS_MAX_STRING_SIZE];
    memset(str, 0, sizeof(str));
    return av_ts_make_time_string(str, ts, &tb);
}

inline std::ostream & operator<<(std::ostream & _out, AVRational const & _av_rational) {
    _out << _av_rational.num << "/" << _av_rational.den;
    return _out;
}


namespace vio {

// * -------------------------------------------------------------------------------------------------------------- * //
// *                                                   Time units                                                   * //
// * -------------------------------------------------------------------------------------------------------------- * //

// define time unit types from std::chrono, with specific memory size
using Nanosecond  = std::chrono::duration<int64_t, std::nano        >;
using Microsecond = std::chrono::duration<int64_t, std::micro       >;
using Millisecond = std::chrono::duration<int64_t, std::milli       >;
using Second      = std::chrono::duration<int64_t                   >;
using Minute      = std::chrono::duration<int64_t, std::ratio<60>   >;
using Hour        = std::chrono::duration<int64_t, std::ratio<3600 >>;
using MsDouble    = std::chrono::duration<double,  std::milli       >;

// literal operator
inline Nanosecond  operator ""_ns  (unsigned long long val) { Nanosecond  ret = Nanosecond ((int64_t)val); return ret; }
inline Microsecond operator ""_us  (unsigned long long val) { Microsecond ret = Microsecond((int64_t)val); return ret; }
inline Millisecond operator ""_ms  (unsigned long long val) { Millisecond ret = Millisecond((int64_t)val); return ret; }
inline Second      operator ""_sec (unsigned long long val) { Second      ret = Second     ((int64_t)val); return ret; }
inline Minute      operator ""_min (unsigned long long val) { Minute      ret = Minute     ((int64_t)val); return ret; }
inline Hour        operator ""_hour(unsigned long long val) { Hour        ret = Hour       ((int64_t)val); return ret; }

using Timestamp = Millisecond;
// define no time stamp
constexpr Timestamp kNoTimestamp(((int64_t)UINT64_C(0x8000000000000000)));
constexpr int64_t kRatioOfSecondToTimestamp = (int64_t)1000000;

template <typename T, typename U, typename U_Period>
inline T cast(const std::chrono::duration<U, U_Period> & _input) {
    return std::chrono::duration_cast<T>(_input);
}

inline Millisecond AVTime2MS(int64_t _av_time, AVRational _time_base = AV_TIME_BASE_Q) {
    return Millisecond(av_rescale_q(_av_time, _time_base, {1, 1000}));
}
inline int64_t MS2AVTime(Millisecond _ts, AVRational _time_base = AV_TIME_BASE_Q) {
    return av_rescale_q(_ts.count(), {1, 1000}, _time_base);
}


// * -------------------------------------------------------------------------------------------------------------- * //
// *                                                  Easy Creation                                                 * //
// * -------------------------------------------------------------------------------------------------------------- * //

#if LIBAVUTIL_VERSION_MAJOR >= 57
AVFrame * AllocateFrame(enum AVSampleFormat sampleFmt, AVChannelLayout const & channelLayout, int sampleRate, int nbSamples);
#else
AVFrame * AllocateFrame(enum AVSampleFormat sampleFmt, uint64_t channelLayout, int sampleRate, int nbSamples);
#endif
AVFrame * AllocateFrame(enum AVPixelFormat pixFmt, int width, int height);

}
