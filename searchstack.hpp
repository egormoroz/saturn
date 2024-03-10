#ifndef SEARCHSTACK_HPP
#define SEARCHSTACK_HPP

#include "primitives/common.hpp"
#include <array>

class Board;

constexpr int MAX_PLIES = 1024;
class Stack {
public:
    struct Entry {
        uint64_t key;
        Move move;
        Move excluded;
        Move killers[2];
        int16_t eval;
    };

    Stack() = default;

    void set_start(int start);
    void reset();

    void push(uint64_t key, Move m = MOVE_NONE, int16_t eval = 0, Move excluded = MOVE_NONE);
    void pop();

    Entry &at(int ply);

    int height() const;
    int total_height() const;
    bool capped() const;

    bool is_repetition(const Board &b, int fold=1) const;

    int16_t mated_score() const;

private:
    std::array<Entry, MAX_PLIES> entries_;
    int height_{}, start_{};
};

#endif
