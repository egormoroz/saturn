#include "tt.hpp"
#include <cstring>
#include "board/board.hpp"
#include <algorithm>
#include <xmmintrin.h>

TranspositionTable g_tt;

int TTEntry::score(int ply) const {
    int s = score16;
    if (s > MATE_BOUND)
        s -= ply;
    else if (s < -MATE_BOUND)
        s += ply;
    return s;
}

TTEntry::TTEntry(uint64_t key, int s, Bound b, 
    int depth, Move m, int ply, bool null) : key(key)
{
    move16 = uint16_t(m);
    depth8 = uint8_t(depth);
    bound8 = uint8_t(b);
    avoid_null = null;

    if (s > MATE_BOUND)
        s += ply;
    else if (s < -MATE_BOUND)
        s -= ply;
    score16 = int16_t(s);
}

void TranspositionTable::resize(size_t mbs) {
    if (buckets_)
        delete[] buckets_;
    size_ = mbs * 1024 * 1024 / sizeof(Bucket);
    buckets_ = new Bucket[size_];
    memset(buckets_, 0, size_ * sizeof(Bucket));
}

void TranspositionTable::clear() {
    if (buckets_)
        memset(buckets_, 0, size_ * sizeof(Bucket));
}

void TranspositionTable::new_search() {
    ++age_;
}

bool TranspositionTable::probe(uint64_t key, 
        TTEntry &e) const 
{
    Bucket &b = buckets_[key % size_];
    for (int i = 0; i < Bucket::N; ++i) {
        e = b.entries[i];
        if ((e.key ^ e.data) == key) {
            e.age = age_;
            return true;
        }
    }

    return false;
}

void TranspositionTable::store(TTEntry new_entry) {
    Bucket &b = buckets_[new_entry.key % size_];
    TTEntry *replace = nullptr;
    for (int i = 0; i < Bucket::N; ++i) {
        TTEntry &e = b.entries[i];
        if ((e.key ^ e.data) == new_entry.key) {
            replace = &e;
            break;
        }
    }

    if (!replace) {
        int replace_depth = 9999;
        for (int i = 0; i < Bucket::N; ++i) {
            TTEntry &e = b.entries[i];
            if (e.age != age_ && e.depth8 < replace_depth) {
                replace = &e;
                replace_depth = e.depth8;
            }
        }

        if (!replace) {
            for (int i = 0; i < Bucket::N; ++i) {
                TTEntry &e = b.entries[i];
                if (e.depth8 < replace_depth) {
                    replace = &e;
                    replace_depth = e.depth8;
                }
            }
        }
    }

    new_entry.age = age_;

    replace->key = new_entry.key ^ new_entry.data;
    replace->data = new_entry.data;
}

void TranspositionTable::prefetch(uint64_t key) const {
    _mm_prefetch((const char*)&buckets_[key % size_], 
            _MM_HINT_NTA);
}

uint64_t TranspositionTable::hashfull() const {
    uint64_t cnt = 0;
    for (size_t i = 0; i < 1000; ++i) {
        for (auto &e: buckets_[i].entries)
            cnt += e.depth8 && e.age == age_;
    }

    return cnt / Bucket::N;
}

TranspositionTable::~TranspositionTable() {
    if (buckets_) {
        delete[] buckets_;
    }
}

int TranspositionTable::extract_pv(Board b, Move *pv, int len) {
    int n = 0;
    TTEntry tte;
    while (probe(b.key(), tte) && n < len) {
        Move m = Move(tte.move16);
        if (!b.is_valid_move(m))
            break;
        b = b.do_move(m);
        *pv++ = m;
        ++n;
    }
    return n;
}

