#pragma once

#include "../primitives/common.hpp"

template<typename T>
constexpr T round_up(T x, T multiple) {
    T remainder = x % multiple;
    if (!remainder)
        return x;

    return x + multiple - remainder;
}

template<typename T>
struct Span {
    Span(T *begin, T *end)
        : begin_(begin), end_(end) {}

    T* begin() { return begin_; }
    T* end() { return end_; }

    size_t size() const { return end_ - begin_; }

private:
    T *begin_;
    T *end_;
};


/* enum */
/* { */
/* 	ps_w_pawn = 1, */
/* 	ps_b_pawn = 1 * 64 + 1, */
/* 	ps_w_knight = 2 * 64 + 1, */
/* 	ps_b_knight = 3 * 64 + 1, */
/* 	ps_w_bishop = 4 * 64 + 1, */
/* 	ps_b_bishop = 5 * 64 + 1, */
/* 	ps_w_rook = 6 * 64 + 1, */
/* 	ps_b_rook = 7 * 64 + 1, */
/* 	ps_w_queen = 8 * 64 + 1, */
/* 	ps_b_queen = 9 * 64 + 1, */
/* 	ps_end = 10 * 64 + 1 */
/* }; */

/* constexpr uint32_t piece_to_index[2][14] = { */ 
/*     { */
/*         0, ps_w_pawn, ps_w_knight, ps_w_bishop, ps_w_rook, ps_w_queen, 0, 0, */ 
/*         0, ps_b_pawn, ps_b_knight, ps_b_bishop, ps_b_rook, ps_b_queen, */
/*     }, */
/*     { */
/*         0, ps_b_pawn, ps_b_knight, ps_b_bishop, ps_b_rook, ps_b_queen, 0, 0, */
/*         0, ps_w_pawn, ps_w_knight, ps_w_bishop, ps_w_rook, ps_w_queen, */
/*     } */
/* }; */

constexpr inline Square orient(Color c, Square sq) {
    return Square(sq ^ (c == WHITE ? 0x0 : 0x3f));
}

/* constexpr uint16_t halfkp_idx(Color persp, Square relksq, Square psq, Piece p) { */
/*     return orient(persp, psq) + piece_to_index[persp][p] + ps_end * relksq; */
/* } */

constexpr int piece_to_index(Color pov, Piece p) {
    return 2 * (type_of(p) - 1) + int(color_of(p) != pov);
}

constexpr uint16_t halfkp_idx(Color pov, Square ksq, Square psq, Piece p) {
    return orient(pov, psq) + 64*piece_to_index(pov, p) + 64*10*ksq;
}
