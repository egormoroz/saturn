#ifndef FTSET_HPP
#define FTSET_HPP

#include "../primitives/common.hpp"
#include "../board/board.hpp"


namespace mini {

constexpr int N_KING_BUCKETS = 4;

constexpr int N_FEATURES = 12 * 64 * N_KING_BUCKETS;
constexpr int MAX_TOTAL_FTS = 32;

constexpr int KING_BUCKETS[] = {
    0, 0, 1, 1, 1, 1, 0, 0,
    2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 
    3, 3, 3, 3, 3, 3, 3, 3, 
    3, 3, 3, 3, 3, 3, 3, 3, 
    3, 3, 3, 3, 3, 3, 3, 3, 
    3, 3, 3, 3, 3, 3, 3, 3, 
    3, 3, 3, 3, 3, 3, 3, 3,
};

constexpr uint16_t index(Color pov, Square sq, Piece p, Square ksq) {
    const uint16_t p_idx = 2 * (type_of(p) - 1) + uint16_t(color_of(p) != pov);
    const uint16_t flip = pov == WHITE ? 0 : 56;
    const uint16_t o_sq = sq ^ flip;
    const uint16_t o_ksq = ksq ^ flip;

    return o_sq + 64*p_idx + 64*12*KING_BUCKETS[o_ksq];
}

constexpr bool same_king_bucket(Color pov, Square s1, Square s2) {
    const uint16_t flip = pov == WHITE ? 0 : 56;
    return KING_BUCKETS[s1 ^ flip] == KING_BUCKETS[s2 ^ flip];
}

inline int get_active_features(const Board &b, Color side, uint16_t *fts) {
    int n_fts = 0;
    Bitboard mask = b.pieces();
    Square ksq = b.king_square(side);

    while (mask) {
        Square psq = pop_lsb(mask);
        fts[n_fts++] = index(side, psq, b.piece_on(psq), ksq);
    }

    return n_fts;
}


} // mini



#endif
