#include "attack.hpp"
#include <initializer_list>

namespace attack_tables {

Bitboard ATTACKS[88772];
Bitboard PSEUDO_ATTACKS[PIECE_TYPE_NB][SQUARE_NB];

Bitboard PAWN_ATTACKS[COLOR_NB][SQUARE_NB];
Bitboard PAWN_PUSHES[COLOR_NB][SQUARE_NB];

Bitboard LINE[SQUARE_NB][SQUARE_NB];
Bitboard BETWEEN[SQUARE_NB][SQUARE_NB];


/* size_t Magic::rook_index(Bitboard blockers) const { */
/*     uint64_t index = (blockers & mask) * factor; */
/*     return (index >> (64 - 12)) + offset; */
/* } */

/* size_t Magic::bishop_index(Bitboard blockers) const { */
/*     uint64_t index = (blockers & mask) * factor; */
/*     return (index >> (64 - 9)) + offset; */
/* } */

} //namespace attack_tables

namespace atta = attack_tables;

/* Bitboard attacks_bb(PieceType pt, Square sq, Bitboard blockers) { */
/*     assert(pt > PAWN && pt <= KING && is_ok(sq)); */
/*     switch (pt) { */
/*     case BISHOP: */
/*         return attacks_bb<BISHOP>(sq, blockers); */
/*     case ROOK: */
/*         return attacks_bb<ROOK>(sq, blockers); */
/*     case QUEEN: */
/*         return attacks_bb<QUEEN>(sq, blockers); */
/*     default: //KING */
/*         return attacks_bb<KING>(sq);; */
/*     }; */
/* } */

/* Bitboard pawn_attacks_bb(Color c, Square sq) { */
/*     assert(is_ok(c) && is_ok(sq)); */
/*     return atta::PAWN_ATTACKS[c][sq]; */

/* } */

/* Bitboard pawn_pushes_bb(Color c, Square sq) { */
/*     assert(is_ok(c) && is_ok(sq)); */
/*     return atta::PAWN_PUSHES[c][sq]; */
/* } */

/* Bitboard line_bb(Square s1, Square s2) { */
/*     assert(is_ok(s1) && is_ok(s2)); */
/*     return atta::LINE[s1][s2]; */
/* } */

/* Bitboard between_bb(Square s1, Square s2) { */
/*     assert(is_ok(s1) && is_ok(s2)); */
/*     return atta::BETWEEN[s1][s2]; */
/* } */


class Dir {
public:
    Dir(Direction dir) : dir_(dir) {}

    int next_index(int index) const {
        int rank = index / 8 + SX[dir_], file = index % 8 + SY[dir_];
        return (rank >= 0 && file >= 0 && rank < 8 && file < 8) ? (rank * 8 + file) : -1;
    }

private:
    Direction dir_;

    static constexpr int SX[DIRS_NB] = { 0, 0, 1, -1, 1, -1, 1, -1 };
    static constexpr int SY[DIRS_NB] = { 1, -1, 0, 0, 1, 1, -1, -1 };
};

static Bitboard gen_attacks(Square sq, Bitboard blockers, Dir d) {
    Bitboard bb = 0;

    for (int i = sq; (i = d.next_index(i)) != -1;) {
        bb |= 1ull << i;
        if (blockers & (1ull << i))
            break;
    }

    return bb;
}

static Bitboard gen_rook_attacks(Square sq, Bitboard blockers) {
    Bitboard bb = 0;
    for (Dir d: { NORTH, SOUTH, EAST, WEST })
        bb |= gen_attacks(sq, blockers, d);

    return bb;
}

static Bitboard gen_bishop_attacks(Square sq, Bitboard blockers) {
    Bitboard bb = 0;
    for (Dir d: { NORTH_EAST, NORTH_WEST, SOUTH_EAST, SOUTH_WEST })
        bb |= gen_attacks(sq, blockers, d);

    return bb;
}

//enumerate all subsets
//https://www.chessprogramming.org/Traversing_Subsets_of_a_Set
template<typename F>
void enum_subsets(F &f, Bitboard d) {
    Bitboard n = 0;
    do {
        f(n);
        n = (n - d) & d;
    } while (n);
}

Bitboard knight_attacks(Bitboard bb) {
   Bitboard l1 = (bb >> 1) & ~FILE_H_BB;
   Bitboard l2 = (bb >> 2) & ~(FILE_H_BB | FILE_G_BB);
   Bitboard r1 = (bb << 1) & ~FILE_A_BB;
   Bitboard r2 = (bb << 2) & ~(FILE_A_BB | FILE_B_BB);
   Bitboard h1 = l1 | r1;
   Bitboard h2 = l2 | r2;
   return (h1<<16) | (h1>>16) | (h2<<8) | (h2>>8);
}

Bitboard king_attacks(Bitboard bb) {
    Bitboard attacks = shift<EAST>(bb) | shift<WEST>(bb);
    bb |= attacks;
    attacks |= shift<NORTH>(bb) | shift<SOUTH>(bb);
    return attacks;
}


template<Color C>
constexpr Bitboard pawn_attacks_bb(Bitboard bb) {
    return C == WHITE ? shift<NORTH_WEST>(bb) | shift<NORTH_EAST>(bb)
        : shift<SOUTH_WEST>(bb) | shift<SOUTH_EAST>(bb);
}

template<Color C>
constexpr Bitboard pawn_pushes_bb(Bitboard bb) {
    Bitboard pushes = 0;
    if (C == WHITE) {
        bb &= ~RANK_8_BB;
        pushes |= (bb & RANK_2_BB) << 16;
        pushes |= bb << 8;
    } else {
        bb &= ~RANK_1_BB;
        pushes |= (bb & RANK_7_BB) >> 16;
        pushes |= bb >> 8;
    }
    return pushes;
}


void init_attack_tables() {
    for (Square sq = SQ_A1; sq <= SQ_H8; ++sq) {
        Bitboard sbb = square_bb(sq);

        auto f = [sq](Bitboard blockers) {
            size_t idx = atta::ROOK_MAGICS[sq].rook_index(blockers);
            atta::ATTACKS[idx] = gen_rook_attacks(sq, blockers);
        };
        enum_subsets(f, atta::ROOK_MAGICS[sq].mask);

        auto g = [sq](Bitboard blockers) {
            size_t idx = atta::BISHOP_MAGICS[sq].bishop_index(blockers);
            atta::ATTACKS[idx] = gen_bishop_attacks(sq, blockers);
        };
        enum_subsets(g, atta::BISHOP_MAGICS[sq].mask);


        atta::PAWN_ATTACKS[WHITE][sq] = pawn_attacks_bb<WHITE>(sbb);
        atta::PAWN_ATTACKS[BLACK][sq] = pawn_attacks_bb<BLACK>(sbb);
        atta::PAWN_PUSHES[WHITE][sq] = pawn_pushes_bb<WHITE>(sbb);
        atta::PAWN_PUSHES[BLACK][sq] = pawn_pushes_bb<BLACK>(sbb);

        atta::PSEUDO_ATTACKS[KNIGHT][sq] = knight_attacks(sbb);
        atta::PSEUDO_ATTACKS[KING][sq] = king_attacks(sbb);

        atta::PSEUDO_ATTACKS[BISHOP][sq] = attacks_bb<BISHOP>(sq, 0);
        atta::PSEUDO_ATTACKS[ROOK][sq] = attacks_bb<ROOK>(sq, 0);
        atta::PSEUDO_ATTACKS[QUEEN][sq] = attacks_bb<QUEEN>(sq, 0);
    }

    for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1) {
        Bitboard s1b = square_bb(s1);
        for (PieceType p: { BISHOP, ROOK }) {
            for (Square s2 = SQ_A1; s2 <= SQ_H8; ++s2) {
                Bitboard s2b = square_bb(s2);

                if (attacks_bb(p, s1, 0) & s2b) {
                    atta::LINE[s1][s2] = (attacks_bb(p, s1, 0) 
                            & attacks_bb(p, s2, 0)) | s1b | s2b;
                    atta::BETWEEN[s1][s2] = attacks_bb(p, s1, s2b) 
                        & attacks_bb(p, s2, s1b);
                }
            }
        }
    }
}

