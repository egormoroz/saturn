#ifndef SEARCH_COMMON_HPP
#define SEARCH_COMMON_HPP

#include <cstdint>
#include <chrono>
#include <algorithm>
#include "../primitives/common.hpp"

struct SearchStats {
    uint64_t nodes{}, qnodes{};
    uint64_t fail_high{}, fail_high_first{};
    int sel_depth{};
    // keep track of iteradtive deepening depth
    int id_depth{};

    void reset() {
        nodes = qnodes = fail_high = fail_high_first = 0;
        sel_depth = 0;
        id_depth = 0;
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
    int min_depth = 0;
    int time[2]{}, inc[2]{};
    int move_time = 0;

    bool infinite = false;

    uint64_t max_nodes = 0;
    TimePoint start{};
};

struct TimeMan {
    TimePoint start = 0;
    TimePoint max_time = 0;

    void init(const SearchLimits &limits, 
            Color us, int move_overhead) 
    {
        start = limits.start;

        if (limits.infinite) return;

        if (limits.move_time) {
            max_time = std::max(1, limits.move_time - move_overhead);
            return;
        }

        max_time = limits.time[us] / 30 + limits.inc[us] - move_overhead;
    }

    bool out_of_time() const { 
        return timer::now() - start >= max_time;
    }
};

#endif
