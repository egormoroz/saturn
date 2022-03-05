#ifndef TIMER_HPP
#define TIMER_HPP

#include <chrono>
#include <atomic>

using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;

class Timer {
public:
    Timer(TimePoint start, int dur_ms)
        : end_(start + std::chrono::milliseconds(dur_ms)),
          out_of_time_(false) {}

    bool out_of_time() {
        if (out_of_time_)
            return true;
        return (out_of_time_ = Clock::now() >= end_);
    }

private:
    TimePoint end_;
    std::atomic_bool out_of_time_;
};

#endif
