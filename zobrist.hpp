#ifndef ZOBRIST_HPP
#define ZOBRIST_HPP

#include "primitives/common.hpp"

struct Zobrist {
    uint64_t psq[PIECE_NB][SQUARE_NB];
    uint64_t enpassant[FILE_NB];
    uint64_t castling[CASTLING_RIGHTS_NB];
    uint64_t side;

    Zobrist();
};

extern Zobrist ZOBRIST;

#endif
