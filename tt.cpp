#include "tt.hpp"
#include <cassert>
#include <cstring>
#include "board/board.hpp"
#include <algorithm>
#include <xmmintrin.h>

TranspositionTable g_tt;

TTEntry::TTEntry(uint64_t key, int s, 
        Bound b, int depth, Move m, bool null) : key(key)
{
    move16 = uint16_t(m);
    score16 = int16_t(s);
    depth8 = uint8_t(depth);
    bound8 = uint8_t(b);
    avoid_null = null;
    padding = 0;
}

TranspositionTable::TranspositionTable()
    : entries_(nullptr), size_(0) {}

void TranspositionTable::init(size_t mbs) {
    assert(entries_ == nullptr);
    size_ = mbs * 1024 * 1024 / sizeof(TTEntry);
    entries_ = new TTEntry[size_];
    memset(entries_, 0, size_ * sizeof(TTEntry));
}

ProbeResult TranspositionTable::probe(uint64_t key, 
        TTEntry &e) const 
{
    e = entries_[key % size_];
    return (e.key ^ e.data) == key ? HASH_HIT : HASH_MISS;
}

void TranspositionTable::store(TTEntry entry) {
    TTEntry &e = entries_[entry.key % size_];
    if (entry.move16 == MOVE_NONE)
        entry.move16 = e.move16;
    if (entry.bound8 == BOUND_EXACT
        || (e.key ^ e.data) != entry.data
        || e.depth8 >= entry.depth8)
    {
        e.key = entry.key ^ entry.data;
        e.data = entry.data;
    }
}

void TranspositionTable::prefetch(uint64_t key) const {
    _mm_prefetch((const char*)&entries_[key % size_], _MM_HINT_NTA);
}

TranspositionTable::~TranspositionTable() {
    if (entries_) {
        delete[] entries_;
    }
}

int TranspositionTable::extract_pv(Board b, Move *pv) {
    int n = 0;
    TTEntry tte;
    while (probe(b.key(), tte) == HASH_HIT && n++ < MAX_MOVES) {
        Move m = Move(tte.move16);
        if (!b.is_valid_move(m))
            break;
        b = b.do_move(m);
        *pv++ = m;
    }
    return n;
}

