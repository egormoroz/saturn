#pragma once

#include "../primitives/common.hpp"
#include "accumulator.hpp"
#include <vector>

struct Delta {
    Piece piece;
    Square from;
    Square to;
};

struct StateInfo {
    Accumulator acc;

    int nb_deltas = 0;
    Delta deltas[3];

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
};

