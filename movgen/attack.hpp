#ifndef MOVGEN_ATTACK_HPP
#define MOVGEN_ATTACK_HPP

#include "../primitives/bitboard.hpp"
#include "../primitives/common.hpp"
#include <cstddef>

namespace attack_tables {

struct Magic {
    uint64_t mask;
    uint64_t factor;
    size_t offset;


size_t rook_index(Bitboard blockers) const {
    uint64_t index = (blockers & mask) * factor;
    return (index >> (64 - 12)) + offset;
}

size_t bishop_index(Bitboard blockers) const {
    uint64_t index = (blockers & mask) * factor;
    return (index >> (64 - 9)) + offset;
}

};

extern Magic ROOK_MAGICS[SQUARE_NB];
extern Magic BISHOP_MAGICS[SQUARE_NB];

extern Bitboard ATTACKS[88772];
extern Bitboard PSEUDO_ATTACKS[PIECE_TYPE_NB][SQUARE_NB];

extern Bitboard PAWN_ATTACKS[COLOR_NB][SQUARE_NB];
extern Bitboard PAWN_PUSHES[COLOR_NB][SQUARE_NB];

extern Bitboard LINE[SQUARE_NB][SQUARE_NB];
extern Bitboard BETWEEN[SQUARE_NB][SQUARE_NB];

} //namespace attack_tables


void init_attack_tables();

template<PieceType pt>
Bitboard attacks_bb(Square sq) {
    static_assert(pt > PAWN && pt <= KING);
    assert(is_ok(sq));
    return attack_tables::PSEUDO_ATTACKS[pt][sq];
}

template<PieceType pt>
Bitboard attacks_bb(Square sq, Bitboard blockers) {
    namespace at = attack_tables;
    static_assert(pt > PAWN && pt <= KING);
    switch (pt) {
    case BISHOP:
        return at::ATTACKS[at::BISHOP_MAGICS[sq].bishop_index(
            blockers)];
    case ROOK:
        return at::ATTACKS[at::ROOK_MAGICS[sq].rook_index(
            blockers)];
    case QUEEN:
        return attacks_bb<BISHOP>(sq, blockers) 
            | attacks_bb<ROOK>(sq, blockers);
    default: //KING
        return attacks_bb<pt>(sq);
    };
}

inline Bitboard attacks_bb(PieceType pt, Square sq, Bitboard blockers) {
    assert(pt > PAWN && pt <= KING && is_ok(sq));
    switch (pt) {
    case BISHOP:
        return attacks_bb<BISHOP>(sq, blockers);
    case ROOK:
        return attacks_bb<ROOK>(sq, blockers);
    case QUEEN:
        return attacks_bb<QUEEN>(sq, blockers);
    default: //KING
        return attacks_bb<KING>(sq);;
    };
}

inline Bitboard pawn_attacks_bb(Color c, Square sq) {
    assert(is_ok(c) && is_ok(sq));
    return attack_tables::PAWN_ATTACKS[c][sq];

}

inline Bitboard pawn_pushes_bb(Color c, Square sq) {
    assert(is_ok(c) && is_ok(sq));
    return attack_tables::PAWN_PUSHES[c][sq];
}

inline Bitboard line_bb(Square s1, Square s2) {
    assert(is_ok(s1) && is_ok(s2));
    return attack_tables::LINE[s1][s2];
}

inline Bitboard between_bb(Square s1, Square s2) {
    assert(is_ok(s1) && is_ok(s2));
    return attack_tables::BETWEEN[s1][s2];
}



#endif
