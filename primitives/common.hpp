#ifndef PRIMITIVES_COMMON_HPP
#define PRIMITIVES_COMMON_HPP

#include "../config.hpp"

#include <cstdint>
#include <cstddef>
#include <cassert>

/*---------Square, file and rank definitions---------*/

enum Square : uint8_t {
  SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
  SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
  SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
  SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
  SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
  SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
  SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
  SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
  SQ_NONE, SQUARE_NB = SQ_NONE
};

enum File : uint8_t {
    FILE_A, FILE_B, FILE_C, FILE_D, 
    FILE_E, FILE_F, FILE_G, FILE_H,
    FILE_NB, FILE_NONE = FILE_NB
};

enum Rank : uint8_t {
    RANK_1, RANK_2, RANK_3, RANK_4,
    RANK_5, RANK_6, RANK_7, RANK_8,
    RANK_NB, RANK_NONE = RANK_NB
};

constexpr File file_of(Square sq) { return File(sq & 7); }
constexpr Rank rank_of(Square sq) { return Rank(sq >> 3); }

constexpr Square make_square(File f, Rank r) { 
    return Square(f + (r << 3));
}

constexpr bool is_ok(Square sq) { return sq >= SQ_A1 && sq <= SQ_H8; }

/*------End of square, file and rank definitions-----*/


/*-------------Color and piece defitions-------------*/

enum Color : uint8_t {
    WHITE,
    BLACK,
    COLOR_NB,
    COLOR_NONE = COLOR_NB,
};

constexpr Color operator~(Color c) { return Color(c ^ 1); }

enum PieceType : uint8_t {
    NO_PIECE_TYPE, //this is not optimal...

    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,

    PIECE_TYPE_NB,
};

constexpr PieceType ALL_PTYPES[6] {
    PAWN, KNIGHT, BISHOP, 
    ROOK, QUEEN, KING,
};

enum Piece : uint8_t {
    NO_PIECE,
    W_PAWN = PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
    B_PAWN = PAWN + 8, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
    PIECE_NB, //this isn't either...
};

constexpr PieceType type_of(Piece p) {
    return PieceType(p & 7);
}

constexpr Color color_of(Piece p) {
    return Color(p >> 3);
}

constexpr Piece make_piece(Color c, PieceType pt) {
    return Piece((c << 3) | pt);
}

constexpr bool is_ok(Color c) {
    return c == WHITE || c == BLACK;
}

constexpr bool is_ok(PieceType pt) {
    return pt >= PAWN && pt <= QUEEN;
}

constexpr bool is_ok(Piece p) {
    return (p >= W_PAWN && p <= W_KING) 
        || (p >= B_PAWN && p <= B_KING);
}

/*---------End of color and piece definitions--------*/


/*-------------Castling rights defitnions------------*/

enum CastlingRights : uint8_t {
    NO_CASTLING,

    WHITE_KINGSIDE,
    WHITE_QUEENSIDE = WHITE_KINGSIDE << 1,
    BLACK_KINGSIDE = WHITE_KINGSIDE << 2,
    BLACK_QUEENSIDE = WHITE_KINGSIDE << 3,

    WHITE_CASTLING = WHITE_KINGSIDE | WHITE_QUEENSIDE,
    BLACK_CASTLING = BLACK_KINGSIDE | BLACK_QUEENSIDE,
    ALL_CASTLING = WHITE_CASTLING | BLACK_CASTLING,

    CASTLING_RIGHTS_NB = 16,
};

constexpr CastlingRights kingside_rights(Color c) {
    return CastlingRights(1 << (c * 2));
}

constexpr CastlingRights queenside_rights(Color c) {
    return CastlingRights(2 << (c * 2));
}

constexpr bool is_ok(CastlingRights cr) {
    return cr < CASTLING_RIGHTS_NB && cr >= 0;
}

/*---------End of castling rights defitnions---------*/


/*-------------Some operation definitions------------*/

#define ENABLE_BASE_OPERATORS_ON(T)                                \
constexpr T operator+(T d1, int d2) { return T(int(d1) + d2); }    \
constexpr T operator-(T d1, int d2) { return T(int(d1) - d2); }    \
constexpr T operator-(T d) { return T(-int(d)); }                  \
constexpr T& operator+=(T& d1, int d2) { return d1 = d1 + d2; }       \
constexpr T& operator-=(T& d1, int d2) { return d1 = d1 - d2; }

#define ENABLE_INCR_OPERATORS_ON(T)                                \
constexpr T& operator++(T& d) { return d = T(int(d) + 1); }           \
constexpr T& operator--(T& d) { return d = T(int(d) - 1); }

#define ENABLE_FULL_OPERATORS_ON(T)                                \
ENABLE_BASE_OPERATORS_ON(T)                                        \
constexpr T operator*(int i, T d) { return T(i * int(d)); }        \
constexpr T operator*(T d, int i) { return T(int(d) * i); }        \
constexpr T operator/(T d, int i) { return T(int(d) / i); }        \
constexpr int operator/(T d1, T d2) { return int(d1) / int(d2); }  \
constexpr T& operator*=(T& d, int i) { return d = T(int(d) * i); }    \
constexpr T& operator/=(T& d, int i) { return d = T(int(d) / i); }

ENABLE_INCR_OPERATORS_ON(Piece)
ENABLE_INCR_OPERATORS_ON(PieceType)
ENABLE_INCR_OPERATORS_ON(Square)
ENABLE_INCR_OPERATORS_ON(File)
ENABLE_INCR_OPERATORS_ON(Rank)

#undef ENABLE_FULL_OPERATORS_ON
#undef ENABLE_INCR_OPERATORS_ON
#undef ENABLE_BASE_OPERATORS_ON

constexpr Rank relative_rank(Color c, Rank r) {
    return Rank(r ^ (c * 7));
}

constexpr Rank relative_rank(Color c, Square s) {
    return relative_rank(c, rank_of(s));
}

constexpr Square relative_square(Color c, Square s) {
    return Square(s ^ (c * 56));
}

constexpr Square sq_forward(Color c, Square s) {
    return Square(c == WHITE ? s + 8 : s - 8);
}

constexpr Square sq_backward(Color c, Square s) {
    return sq_forward(~c, s);
}

constexpr Square sq_mirror(Square s) { return Square(s ^ 56); }

/*----------End of some operation definitions--------*/


/*------------------Move defitions-------------------*/

enum Move : uint16_t {
    MOVE_NONE,
    MOVE_NULL = 0xFFFF,
};

enum MoveType : uint16_t {
    NORMAL,
    PROMOTION = 1 << 14,
    EN_PASSANT = 2 << 14,
    CASTLING = 3 << 14,
};

constexpr int from_to(Move m) {
    return m & 0xFFF;
}

constexpr Square from_sq(Move m) {
    return Square((m >> 6) & 0x3F);
}

constexpr Square to_sq(Move m) {
    return Square(m & 0x3F);
}

constexpr MoveType type_of(Move m) {
    return MoveType(m & (3 << 14));
}

constexpr PieceType prom_type(Move m) {
    return PieceType(((m >> 12) & 3) + KNIGHT);
}

constexpr Move make_move(Square from, Square to) {
    return Move((from << 6) | to);
}

template<MoveType MT>
Move make(Square from, Square to, PieceType prom = KNIGHT) {
    return Move(MT | ((prom - KNIGHT) << 12) | (from << 6) | to);
}

constexpr bool is_ok(Move m) {
    return from_sq(m) != to_sq(m);
}

/*--------------End of move defitions----------------*/


/*-------------Various values definitions------------*/

enum : int16_t {
    VALUE_ZERO = 0,
    VALUE_MATE = 32000,
    MATE_BOUND = 30000,
};

constexpr int mate_in(int ply) { return VALUE_MATE - ply; }
constexpr int mated_in(int ply) { return -VALUE_MATE + ply; }

constexpr int MAX_DEPTH = 64;
constexpr int MAX_MOVES = 224;

/*----------End of various values definitions--------*/


/*--------------Direction definitions----------------*/

enum Direction { 
    NORTH, SOUTH, EAST, WEST, 
    NORTH_EAST, NORTH_WEST, 
    SOUTH_EAST, SOUTH_WEST, DIRS_NB
};

template<Direction D>
constexpr Square sq_shift(Square sq) {
    switch (D) {
    case NORTH:
        return Square(sq + 8);
    case SOUTH:
        return Square(sq - 8);
    case WEST:
        return Square(sq - 1);
    case EAST:
        return Square(sq + 1);
    case NORTH_WEST:
        return Square(sq + 7);
    case NORTH_EAST:
        return Square(sq + 9);
    case SOUTH_WEST:
        return Square(sq - 9);
    case SOUTH_EAST:
        return Square(sq - 7);
    default:
        return sq;
    };
}

/*--------------End of direction definitions---------*/

#endif
