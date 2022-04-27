#pragma once
#include <chrono>
#include <string>
#include <iostream>


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
