#ifndef SEARCHSTACK_HPP
#define SEARCHSTACK_HPP

#include "primitives/common.hpp"
#include <vector>

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

    Stack();

    void set_start(int start);
    void reset();

    void push(uint64_t key, Move m = MOVE_NONE, int16_t eval = 0, Move excluded = MOVE_NONE);
    void pop();

    Entry &at(int ply);

    int height() const;
    int total_height() const;
    bool capped() const;

    bool is_repetition(const Board &b) const;

    int16_t mated_score() const;

private:
    std::vector<Entry> entries_;
    //std::array<Entry, MAX_PLIES> entries_;
    int height_{}, start_{};
};

#endif
