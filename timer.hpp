#ifndef TIMER_HPP
#define TIMER_HPP

#include <chrono>
#include <atomic>

using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;

class Timer {
public:
    Timer() = default;
    Timer(TimePoint start, int dur_ms)
        : start_(start), 
          end_(start + std::chrono::milliseconds(dur_ms)),
          out_of_time_(false) {}

    bool out_of_time() {
        if (out_of_time_)
            return true;
        return (out_of_time_ = Clock::now() >= end_);
    }

    void restart(TimePoint start, int dur_ms) {
        start_ = start;
        end_ = start + std::chrono::milliseconds(dur_ms);
        out_of_time_ = false;
    }

    int elapsed_millis() const {
        namespace chrono = std::chrono;
        return static_cast<int>(chrono::duration_cast<chrono
                ::milliseconds>(Clock::now() - start_).count());
    }

private:
    TimePoint start_, end_;
    std::atomic_bool out_of_time_{true};
};

#endif
