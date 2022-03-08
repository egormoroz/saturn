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
}

void TranspositionTable::init(size_t mbs) {
    assert(buckets_ == nullptr);
    size_ = mbs * 1024 * 1024 / sizeof(Bucket);
    buckets_ = new Bucket[size_];
    memset(buckets_, 0, size_ * sizeof(Bucket));
}

void TranspositionTable::new_search() {
    ++age_;
}

ProbeResult TranspositionTable::probe(uint64_t key, 
        TTEntry &e) const 
{
    Bucket &b = buckets_[key % size_];
    for (int i = 0; i < Bucket::N; ++i) {
        e = b.entries[i];
        if ((e.key ^ e.data) == key) {
            e.age = age_;
            return HASH_HIT;
        }
    }

    return HASH_MISS;
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

TranspositionTable::~TranspositionTable() {
    if (buckets_) {
        delete[] buckets_;
    }
}

int TranspositionTable::extract_pv(Board b, Move *pv, int len) {
    int n = 0;
    TTEntry tte;
    while (probe(b.key(), tte) == HASH_HIT && n < len) {
        Move m = Move(tte.move16);
        if (!b.is_valid_move(m))
            break;
        b = b.do_move(m);
        *pv++ = m;
        ++n;
    }
    return n;
}

