#ifndef STATE_HPP
#define STATE_HPP

#include "simd.hpp"
#include "../primitives/common.hpp"
#include <array>

#include <cassert>


namespace mini {

constexpr int N_HIDDEN = 256;

struct Accumulator {
    alignas(SIMD_ALIGN) std::array<int16_t, N_HIDDEN> v[2];
    bool computed[2]{};
    int32_t psqt[2];
};


struct Delta {
    Piece piece;
    Square from;
    Square to;
};

struct StateInfo {
    alignas(SIMD_ALIGN) Accumulator acc;

    Delta deltas[3];
    int nb_deltas = 0;

    StateInfo *previous = nullptr;

    void move_piece(Piece p, Square from, Square to) {
        assert(nb_deltas < 3);
        deltas[nb_deltas++] = { p, from, to };
    }

    void add_piece(Piece p, Square s) {
        move_piece(p, SQ_NONE, s);
    }

    void remove_piece(Piece p, Square s) {
        move_piece(p, s, SQ_NONE);
    }

    void reset() {
        previous = nullptr;
        nb_deltas = 0;
        acc.computed[WHITE] = false;
        acc.computed[BLACK] = false;
    }
};

 
} // mini


#endif
