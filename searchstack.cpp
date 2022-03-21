#include "searchstack.hpp"

void Stack::set_start(int start) { 
    start_ = start; 
}

void Stack::reset() {
    start_ = 0;
    height_ = 0;
}

void Stack::push(uint64_t key, Move m, int16_t eval) {
    entries_[height_++] = { key, m, eval };
}

void Stack::pop() {
    assert(height_ > start_);
    height_--;
}

const Stack::Entry& Stack::top() const {
    return entries_[height_ - 1];
}

int Stack::height() const {
    return height_ - start_;
}

bool Stack::is_repetition(int halfmoves) const {
    if (!height_)
        return false;

    uint64_t key = top().key;
    int k = std::max(0, height_ - halfmoves);
    for (int i = halfmoves - 2; i >= k; i -= 2)
        if (entries_[i].key == key)
            return true;
    return false;
}

int16_t Stack::mated_score() const {
    return mated_in(height());
}

