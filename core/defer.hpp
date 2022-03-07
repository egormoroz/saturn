#ifndef DEFER_HPP
#define DEFER_HPP

/*
 * Some helpers for simplified ABDADA
 * http://www.tckerrigan.com/Chess/Parallel_Search/Simplified_ABDADA/simplified_abdada.html
 *
 * */

#include "../primitives/common.hpp"
#include <cstdint>

namespace abdada {

inline uint32_t move_hash(uint64_t pos_hash, Move m) {
    return static_cast<uint32_t>(
        pos_hash ^ ((uint64_t(m) * 1664525) * 1013904223)
    );
}

bool defer_move(uint32_t move_hash, int depth);

void starting_search(uint32_t move_hash, int depth);
void finished_search(uint32_t move_hash, int depth);

} //namespace abdada

#endif
