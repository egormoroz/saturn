#ifndef MOVGEN_GENERATE_HPP
#define MOVGEN_GENERATE_HPP

#include "../primitives/common.hpp"
#include <cstdint>
#include <array>

struct ExtMove {
    uint16_t move;
    uint16_t value;

    operator Move() const { return Move(move); }
    void operator=(Move m) { move = m; }

    operator float() const = delete;
    operator int() const = delete;
    operator unsigned() const = delete;

    bool operator<(const ExtMove &other) const {
        return value < other.value;
    }
};

enum GenType {
    CAPTURES = 1, //and promotions
    QUIET = 2,
    LEGAL = 3,
};

class Board;

template<GenType T>
ExtMove* generate(const Board &b, ExtMove *moves);

#endif
