#ifndef SEARCH_COMMON_HPP
#define SEARCH_COMMON_HPP

#include <cstdint>
#include <chrono>
#include "../primitives/common.hpp"
#include "../parameters.hpp"

struct SearchStats {
    uint64_t nodes{}, qnodes{};
    uint64_t fail_high{}, fail_high_first{};
    int sel_depth{};
    // keep track of iteradtive deepening depth
    int id_depth{};

    void reset() {
        nodes = qnodes = fail_high = fail_high_first = 0;
        sel_depth = id_depth = 0;
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
    enum LimitType {
        UNLIMITED,
        NODES,
        DEPTH,
        TIME,
    } type = UNLIMITED;

    TimePoint start{};

    int depth = MAX_DEPTH;
    int time[2]{}, inc[2]{};
    int move_time = 0;

    uint64_t nodes = 0;
};

struct TimeMan {
    TimePoint start = 0;
    TimePoint max_time = 0;

    void init(const SearchLimits &limits, Color us) {
        start = limits.start;
        if (limits.type != limits.TIME)
            return;

        max_time = limits.move_time;
        max_time += limits.time[us] / 30 + limits.inc[us] - params::move_overhead;
    }

    bool out_of_time() const { 
        return timer::now() - start >= max_time;
    }
};

#endif
