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

constexpr uint16_t halfkp_idx(int ksq, int psq, Piece p) {
    return static_cast<uint16_t>(
        (ksq * 64 + psq) * 10 + (type_of(p) - 1) * 2 + color_of(p)
    );
}

