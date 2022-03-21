#ifndef MOVGEN_GENERATE_HPP
#define MOVGEN_GENERATE_HPP

#include "../primitives/common.hpp"
#include <cstdint>
#include <array>

struct ExtMove {
    Move move;
    int16_t value;

    operator Move() const { return move; }
    void operator=(Move m) { move = m; }

    operator float() const = delete;
    operator int() const = delete;
    operator unsigned() const = delete;

    bool operator<(const ExtMove &other) const {
        return value < other.value;
    }
};

enum GenType {
    TACTICAL = 1, //and promotions
    NON_TACTICAL = 2,
    LEGAL = 3,
};

class Board;

template<GenType T>
ExtMove* generate(const Board &b, ExtMove *moves);

#endif
