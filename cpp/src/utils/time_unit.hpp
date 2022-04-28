#pragma once
#include <chrono>

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
inline T cast(const std::chrono::duration<U, U_Period> & _input)
{
    return std::chrono::duration_cast<T>(_input);
}
