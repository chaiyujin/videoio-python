#pragma once
#include <chrono>
#include <string>
#include <iostream>
#include "log.hpp"

/* ------------------------------------------------------------------------------------------------------------------ */
/*                                    A simple timer don't consider context swaping                                   */
/* ------------------------------------------------------------------------------------------------------------------ */

class Timer
{
public:
    using Clock     = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    Timer() { restart(); }
    void restart() { start_ = Clock::now(); }
    // ms
    double duration() const
    {
        auto m = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start_).count();
        return (double)m / 1000.0;
    }

protected:
    TimePoint start_;
};


class Timeit : public Timer
{
public:
    Timeit(std::string _tag="", bool _verbose=true)
        : tag_(_tag), stop_(false), verbose_(_verbose), duration_(0) {}
    ~Timeit() { stop(); }

    double stop()
    {
        if (stop_) return duration_;

        duration_ = duration();
        if (verbose_)
        {
            int hours = (int)std::floor(duration_ / (3600000.0));
            int mins  = (int)std::floor((duration_ - hours * 3600000.0) / 60000.0);
            int secs  = (int)std::floor((duration_ - hours * 3600000.0 - mins * 60000.0) / 1000.0);
            double ms = (duration_ - hours * 3600000.0 - mins * 60000.0 - secs * 1000.0);
            if      (hours > 0) snow::log::info("[timeit<{}>]: {:2d}h {:2d}m {:2d}s {:.3f}ms", tag_, hours, mins, secs, ms);
            else if (mins > 0)  snow::log::info("[timeit<{}>]: {:2d}m {:2d}s {:.3f}ms", tag_, mins, secs, ms);
            else if (secs > 0)  snow::log::info("[timeit<{}>]: {:2d}s {:.3f}ms", tag_, secs, ms);
            else                snow::log::info("[timeit<{}>]: {:.3f}ms", tag_, ms);
        }

        stop_ = true;
        return duration_;
    }

private:
    std::string tag_;
    bool        stop_;
    bool        verbose_;
    double      duration_;
};
