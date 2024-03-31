#ifndef MOVGEN_ATTACK_HPP
#define MOVGEN_ATTACK_HPP

#include "../primitives/bitboard.hpp"
#include "../primitives/common.hpp"
#include "magics.hpp"

struct AttackTables {
    Bitboard attacks[88772];
    Bitboard pseudo_attacks[PIECE_TYPE_NB][SQUARE_NB];

    Bitboard pawn_attacks[COLOR_NB][SQUARE_NB];
    Bitboard pawn_pushes[COLOR_NB][SQUARE_NB];

    Bitboard line[SQUARE_NB][SQUARE_NB];
    Bitboard between[SQUARE_NB][SQUARE_NB];

    AttackTables();
};

extern AttackTables ATTACK_TABLES;


template<PieceType pt>
Bitboard attacks_bb(Square sq) {
    static_assert(pt > PAWN && pt <= KING);
    assert(is_ok(sq));
    return ATTACK_TABLES.pseudo_attacks[pt][sq];
}

// TODO: implement alternative version using BMI2
template<PieceType pt>
Bitboard attacks_bb(Square sq, Bitboard blockers) {
    static_assert(pt > PAWN && pt <= KING);
    switch (pt) {
    case BISHOP:
        return ATTACK_TABLES.attacks[BISHOP_MAGICS[sq].bishop_index(
            blockers)];
    case ROOK:
        return ATTACK_TABLES.attacks[ROOK_MAGICS[sq].rook_index(
            blockers)];
    case QUEEN:
        return attacks_bb<BISHOP>(sq, blockers) 
            | attacks_bb<ROOK>(sq, blockers);
    default: //KING
        return attacks_bb<pt>(sq);
    };
}

// TODO: implement alternative version using BMI2
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
    return ATTACK_TABLES.pawn_attacks[c][sq];
}

inline Bitboard pawn_pushes_bb(Color c, Square sq) {
    assert(is_ok(c) && is_ok(sq));
    return ATTACK_TABLES.pawn_pushes[c][sq];
}

inline Bitboard line_bb(Square s1, Square s2) {
    assert(is_ok(s1) && is_ok(s2));
    return ATTACK_TABLES.line[s1][s2];
}

inline Bitboard between_bb(Square s1, Square s2) {
    assert(is_ok(s1) && is_ok(s2));
    return ATTACK_TABLES.between[s1][s2];
}



#endif
