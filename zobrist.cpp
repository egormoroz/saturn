#include "zobrist.hpp"
#include <random>

Zobrist ZOBRIST;


Zobrist::Zobrist() {
    std::mt19937 rng(0xdeadbeef);
    std::uniform_int_distribution<uint64_t> dist;

    for (Piece p = NO_PIECE; p < PIECE_NB; ++p)
        for (Square s = SQ_A1; s <= SQ_H8; ++s)
            psq[p][s] = dist(rng);

    for (CastlingRights cr = NO_CASTLING; cr < CASTLING_RIGHTS_NB; cr = CastlingRights(cr + 1))
        castling[cr] = dist(rng);

    for (File f = FILE_A; f <= FILE_H; ++f)
        enpassant[f] = dist(rng);

    side = dist(rng);
}

