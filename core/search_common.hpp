#ifndef SEARCH_COMMON_HPP
#define SEARCH_COMMON_HPP

#include <cstdint>
#include <chrono>
#include "../primitives/common.hpp"

struct SearchStats {
    uint64_t nodes{}, qnodes{};
    uint64_t fail_high{}, fail_high_first{};
    int sel_depth{};

    void reset() {
        nodes = qnodes = fail_high = fail_high_first = 0;
        sel_depth = 0;
    }
};

using TimePoint = std::chrono::milliseconds::rep;
namespace timer {
    inline TimePoint now() {
        auto time = std::chrono::steady_clock::now()
            .time_since_epoch();
        return std::chrono::duration_cast<
            std::chrono::milliseconds>(time).count();
    }
}

struct SearchLimits {
    int max_depth = MAX_DEPTH;
    int time[2]{}, inc[2]{};
    int move_time{};
    bool infinite{};

    TimePoint start{};
};

struct TimeMan {
    TimePoint start;
    TimePoint max_time;

    void init(const SearchLimits &limits, 
            Color us, int ply) 
    {
        (void)(ply);
        if (limits.infinite) return;
        if (limits.move_time) {
            max_time = limits.move_time;
            return;
        }

        int time = limits.time[us] / 45 
            + limits.inc[us];
        time = std::min(time, limits.time[us] * 9 / 10);
        max_time = time;
    }

    bool out_of_time() const { 
        return timer::now() - start >= max_time;
    }
};

#endif
