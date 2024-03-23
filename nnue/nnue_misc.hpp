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


class Board;

namespace halfkp {

constexpr int N_SQ = 64;
constexpr int N_PC = 10;

constexpr int N_VIRT_FT = N_SQ * N_PC;

constexpr int MAX_REAL_FTS = 30;
constexpr int MAX_VIRT_FTS = 30;
constexpr int MAX_TOTAL_FTS = MAX_REAL_FTS + MAX_VIRT_FTS;

constexpr int N_KING_BUCKETS = N_SQ / 2;
constexpr int N_FT = N_SQ * N_PC * N_KING_BUCKETS;

constexpr int KING_BUCKETS[] = {
    -1, -1, -1, -1,  3,  2,  1,  0,
    -1, -1, -1, -1,  7,  6,  5,  4,
    -1, -1, -1, -1, 11, 10,  9,  8,
    -1, -1, -1, -1, 15, 14, 13, 12,
    -1, -1, -1, -1, 19, 18, 17, 16,
    -1, -1, -1, -1, 23, 22, 21, 20,
    -1, -1, -1, -1, 27, 26, 25, 24,
    -1, -1, -1, -1, 31, 30, 29, 28,
};

constexpr inline Square orient(Color pov, Square sq, Square ksq) {
    const int file = file_of(ksq);
    const int hor_flip = 7 * (file < 4);
    const int vert_flip = pov == WHITE ? 0x0 : 56;

    return Square(sq ^ vert_flip ^ hor_flip);
}

constexpr int piece_to_index(Color pov, Piece p) {
    return 2 * (type_of(p) - 1) + int(color_of(p) != pov);
}

constexpr uint16_t halfkp_idx(Color pov, Square ksq, Square psq, Piece p) {
    const int pc_idx = piece_to_index(pov, p);
    const int o_ksq = orient(pov, ksq, ksq);
    const int o_sq = orient(pov, psq, ksq);

    return o_sq + N_SQ * pc_idx + N_SQ*N_PC * KING_BUCKETS[o_ksq];
}

}

