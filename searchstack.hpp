#ifndef SEARCHSTACK_HPP
#define SEARCHSTACK_HPP

#include "primitives/common.hpp"
#include <array>

class Board;

constexpr int MAX_PLIES = 512;
class Stack {
public:
    struct Entry {
        uint64_t key;
        Move move;
        Move killers[2];
        Move excluded;
        int16_t eval;
    };

    Stack() = default;

    void set_start(int start);
    void reset();

    void push(uint64_t key, Move m = MOVE_NONE, int16_t eval = 0);
    void pop();

    Entry &at(int ply);

    int height() const;
    int total_height() const;
    bool capped() const;

    bool is_repetition(const Board &b) const;

    int16_t mated_score() const;

private:
    std::array<Entry, MAX_PLIES> entries_;
    int height_{}, start_{};
};

#endif
