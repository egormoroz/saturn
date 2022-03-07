#include "defer.hpp"

namespace {
    constexpr int DEFER_DEPTH = 3;
    constexpr uint32_t CS_SIZE = 32768;
    constexpr int CS_WAYS = 4;

    volatile uint32_t CURRENTLY_SEARCHING[CS_SIZE][CS_WAYS];
}

namespace abdada {

bool defer_move(uint32_t move_hash, int depth){
    if (depth < DEFER_DEPTH)
        return false;
    uint32_t n = move_hash & (CS_SIZE - 1);

    for (int i = 0; i < CS_WAYS; ++i)
        if (CURRENTLY_SEARCHING[n][i] == move_hash)
            return true;

    return false;
}

void starting_search( uint32_t move_hash, int depth) {
    if (depth < DEFER_DEPTH)
        return;

    uint32_t n = move_hash & (CS_SIZE - 1);
    for (int i = 0; i < CS_WAYS; ++i) {
        if (!CURRENTLY_SEARCHING[n][i]) {
            CURRENTLY_SEARCHING[n][i] = move_hash;
            return;
        }

        if (CURRENTLY_SEARCHING[n][i] == move_hash)
            return;
    }

    CURRENTLY_SEARCHING[n][0] = move_hash;
}

void finished_search( uint32_t move_hash, int depth) {
    if (depth < DEFER_DEPTH)
        return;

    uint32_t n = move_hash & (CS_SIZE - 1);
    for (int i = 0; i < CS_WAYS; ++i)
        if (CURRENTLY_SEARCHING[n][i] == move_hash)
            CURRENTLY_SEARCHING[n][i] = 0;
}

} //namespace abdada

