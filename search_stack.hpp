#ifndef HISTORY_HPP
#define HISTORY_HPP

#include "primitives/common.hpp"
#include <algorithm>

//in plies
constexpr int MAX_GAME_LEN = 512;

struct SearchStack {
    struct Entry {
        uint64_t key;
        Move m;
    } entries[MAX_GAME_LEN];
    int ply{};

    void push(uint64_t key, Move m) {
        entries[ply++] = {key, m};
    }

    void pop() {
        assert(ply > 0);
        --ply;
    }

    Move last_move() const {
        return ply > 0 ? entries[ply - 1].m : MOVE_NONE;
    }

    bool is_repetition(uint64_t key, int fifty) const {
        int k = std::max(0, ply - fifty);

        for (int i = ply - 2; i >= k; i -= 2)
            if (entries[i].key == key)
                return true;
        return false;
    }
};

#endif
