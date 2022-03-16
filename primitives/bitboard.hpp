#ifndef PRIMITIVE_BITBOARD_HPP
#define PRIMITIVE_BITBOARD_HPP

#include <cstdint>
#include "common.hpp"

#ifdef _MSC_VER
#pragma warning(disable:4146)
#include <intrin.h>
#endif


using Bitboard = uint64_t;

/*---------------Define some intrisincs--------------*/

#if defined(__GNUC__)  // GCC, Clang, ICC

constexpr Square lsb(Bitboard b) {
  assert(b);
  return Square(__builtin_ctzll(b));
}

constexpr Square msb(Bitboard b) {
  assert(b);
  return Square(63 ^ __builtin_clzll(b));
}

#elif defined(_MSC_VER)  // MSVC

#ifdef _WIN64  // MSVC, WIN64

inline Square lsb(Bitboard b) {
    assert(b);
    unsigned long idx;
    _BitScanForward64(&idx, b);
    return (Square) idx;
}

inline Square msb(Bitboard b) {
    assert(b);
    unsigned long idx;
    _BitScanReverse64(&idx, b);
    return (Square) idx;
}
#endif // WIN64
#endif // MSVC

inline int popcnt(Bitboard b) {
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
  return (int)__popcnt64(b);
#else // Assumed gcc or compatible compiler
  return __builtin_popcountll(b);
#endif
}

/*-------------End of intrisinc defitions------------*/


/*--------------Basic bitboard defitions-------------*/

constexpr Bitboard EMPTY_BB = Bitboard(0);

constexpr Bitboard RANK_1_BB = 0xFF << 0;
constexpr Bitboard RANK_2_BB = RANK_1_BB << 8;
constexpr Bitboard RANK_3_BB = RANK_1_BB << 16;
constexpr Bitboard RANK_4_BB = RANK_1_BB << 24;
constexpr Bitboard RANK_5_BB = RANK_1_BB << 32;
constexpr Bitboard RANK_6_BB = RANK_1_BB << 40;
constexpr Bitboard RANK_7_BB = RANK_1_BB << 48;
constexpr Bitboard RANK_8_BB = RANK_1_BB << 56;

constexpr Bitboard FILE_A_BB = 0x101010101010101;
constexpr Bitboard FILE_B_BB = FILE_A_BB << 1;
constexpr Bitboard FILE_C_BB = FILE_A_BB << 2;
constexpr Bitboard FILE_D_BB = FILE_A_BB << 3;
constexpr Bitboard FILE_E_BB = FILE_A_BB << 4;
constexpr Bitboard FILE_F_BB = FILE_A_BB << 5;
constexpr Bitboard FILE_G_BB = FILE_A_BB << 6;
constexpr Bitboard FILE_H_BB = FILE_A_BB << 7;

constexpr Bitboard RANK_BB[RANK_NB] = {
    RANK_1_BB, RANK_2_BB, RANK_3_BB, RANK_4_BB,
    RANK_5_BB, RANK_6_BB, RANK_7_BB, RANK_8_BB
};

constexpr Bitboard FILE_BB[FILE_NB] = {
    FILE_A_BB, FILE_B_BB, FILE_C_BB, FILE_D_BB,
    FILE_E_BB, FILE_F_BB, FILE_G_BB, FILE_H_BB
};

constexpr Bitboard KINGSIDE_BB[COLOR_NB] = { 
    0b10010000ull, 0b10010000ull << 56
};

constexpr Bitboard QUEENSIDE_BB[COLOR_NB] = { 
    0b10001ull, 0b10001ull << 56,
};


constexpr Bitboard KINGSIDE_MASK[COLOR_NB] {
    1ull << SQ_F1 | 1ull << SQ_G1,
    1ull << SQ_F8 | 1ull << SQ_G8,
};

constexpr Bitboard QUEENSIDE_MASK[COLOR_NB] {
    1ull << SQ_B1 | 1ull << SQ_C1 | 1ull << SQ_D1,
    1ull << SQ_B8 | 1ull << SQ_C8 | 1ull << SQ_D8,
};


/*----------End of basic bitboard defitions----------*/


/*---------------Bitboard operations-----------------*/

//least significant square
constexpr Bitboard lss_bb(Bitboard bb) {
    assert(bb);
    return bb & -bb;
}

inline Square pop_lsb(Bitboard &bb) {
    Square sq = lsb(bb);
    bb &= bb - 1;
    return sq;
}

constexpr Bitboard square_bb(Square sq) {
    assert(sq < SQUARE_NB);
    return Bitboard(1ull << sq);
}

constexpr Bitboard rank_bb(Rank r) { return RANK_1_BB << (8 * r); }
constexpr Bitboard file_bb(File f) { return FILE_A_BB << f; }

constexpr Bitboard sq_rank_bb(Square s) { return rank_bb(rank_of(s)); }
constexpr Bitboard sq_file_bb(Square s) { return file_bb(file_of(s)); }

template<Direction D>
constexpr Bitboard shift(Bitboard b) {
  return  D == NORTH      ?  b           << 8 : D == SOUTH      ?  b           >> 8
        : D == EAST       ? (b & ~FILE_H_BB) << 1 : D == WEST       ? (b & ~FILE_A_BB) >> 1
        : D == NORTH_EAST ? (b & ~FILE_H_BB) << 9 : D == NORTH_WEST ? (b & ~FILE_A_BB) << 7
        : D == SOUTH_EAST ? (b & ~FILE_H_BB) >> 7 : D == SOUTH_WEST ? (b & ~FILE_A_BB) >> 9
        : 0;
}

constexpr Bitboard adjacent_files_bb(File f) {
    Bitboard bb = file_bb(f);
    return shift<WEST>(bb) | shift<EAST>(bb);
}

constexpr Bitboard relative_rank_bb(Color c, Rank r) {
    return rank_bb(relative_rank(c, r));
}

constexpr Bitboard relative_rank_bb(Color c, Square s) {
    return rank_bb(relative_rank(c, s));
}

/*------------End of bitboard operations-------------*/

#endif
