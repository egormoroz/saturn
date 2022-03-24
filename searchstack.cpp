#include "searchstack.hpp"

void Stack::set_start(int start) { 
    start_ = start; 
}

void Stack::reset() {
    start_ = 0;
    height_ = 0;
    memset(entries_.data(), 0, sizeof(entries_));
}

void Stack::push(uint64_t key, Move m, int16_t eval) {
    entries_[height_++] = { 
        key, m, 
        { MOVE_NONE, MOVE_NONE }, 
        eval 
    };
}

void Stack::pop() {
    assert(height_ > start_);
    height_--;
}

Stack::Entry &Stack::at(int ply) {
    return entries_[start_ + ply];
}

int Stack::height() const { return height_ - start_; }
bool Stack::capped() const { return height_ >= MAX_PLIES; }
int Stack::total_height() const { return height_; }

bool Stack::is_repetition(uint64_t key, int halfmoves) const {
    if (!height_)
        return false;

    int k = std::max(0, height_ - halfmoves);
    for (int i = height_ - 2; i >= k; i -= 2)
        if (entries_[i].key == key)
            return true;
    return false;
}

int16_t Stack::mated_score() const {
    return mated_in(height());
}

