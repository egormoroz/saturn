#include "tt.hpp"
#include <cstring>
#include "board/board.hpp"
#include "parameters.hpp"
#include <xmmintrin.h>

TranspositionTable g_tt;


TranspositionTable::TranspositionTable() {
    resize(params::defaults::tt_size);
}

int TTEntry::score(int ply) const {
    int s = score16;
    if (s > MATE_BOUND)
        s -= ply;
    else if (s < -MATE_BOUND)
        s += ply;
    return s;
}

TTEntry::TTEntry(uint16_t key16, int s, int e, Bound b, 
    int depth, Move m, int ply, bool null, uint8_t age)
{
    assert(int(b) < BOUND_NUM);

    this->key16 = key16;

    move16 = uint16_t(m);
    depth5 = uint8_t(std::min(depth, 63));
    bound2 = uint8_t(b);
    avoid_null = null;

    if (s > MATE_BOUND)
        s += ply;
    else if (s < -MATE_BOUND)
        s -= ply;
    score16 = int16_t(s);
    eval16 = int16_t(e);

    this->age = age;
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

bool TranspositionTable::probe(uint64_t key, TTEntry &e) const {
    uint64_t key16 = uint16_t(key);

    Bucket &b = buckets_[bucket_by_key(key)];
    for (int i = 0; i < Bucket::N; ++i) {
        e = b.entries[i];
        if (e.key16 == key16) {
            e.age = age_;
            return true;
        }
    }

    return false;
}

void TranspositionTable::store(uint64_t key, int score, int eval, 
        Bound bnd, int depth, Move m, int ply, bool avoid_null)
{
    uint16_t key16 = uint16_t(key);
    Bucket &b = buckets_[bucket_by_key(key)];

    TTEntry *replace = nullptr;
    for (int i = 0; i < Bucket::N; ++i) {
        TTEntry &e = b.entries[i];
        if (e.key16 == key16) {
            replace = &e;
            break;
        }
    }

    if (!replace) {
        int replace_depth = 9999;
        for (int i = 0; i < Bucket::N; ++i) {
            TTEntry &e = b.entries[i];
            if (e.age != age_ && e.depth5 < replace_depth) {
                replace = &e;
                replace_depth = e.depth5;
            }
        }

        if (!replace) {
            for (int i = 0; i < Bucket::N; ++i) {
                TTEntry &e = b.entries[i];
                if (e.depth5 < replace_depth) {
                    replace = &e;
                    replace_depth = e.depth5;
                }
            }
        }
    }

    *replace = TTEntry(key16, score, eval, bnd, depth, m, ply, avoid_null, age_);
}

void TranspositionTable::prefetch(uint64_t key) const {
    _mm_prefetch((const char*)&buckets_[key % size_], 
            _MM_HINT_NTA);
}

uint64_t TranspositionTable::hashfull() const {
    uint64_t cnt = 0;
    for (size_t i = 0; i < 1000; ++i) {
        for (auto &e: buckets_[i].entries)
            cnt += e.depth5 && e.age == age_;
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
    StateInfo si;
    while (probe(b.key(), tte) && n < len) {
        Move m = Move(tte.move16);
        if (!b.is_valid_move(m))
            break;
        b = b.do_move(m, &si);
        *pv++ = m;
        ++n;
    }
    return n;
}

void TranspositionTable::extract_pv(Board b, PVLine &pv, 
        int max_len, Move first_move) 
{
    TTEntry tte;
    StateInfo si;

    max_len = std::min(max_len, PVLine::MAX_LEN);

    if (is_ok(first_move)) {
        pv.len = 1;
        pv.moves[0] = first_move;
        b = b.do_move(first_move, &si);
    } else {
        pv.len = 0;
    }

    while (pv.len < max_len && probe(b.key(), tte)) {
        Move m = Move(tte.move16);
        if (!b.is_valid_move(m))
            break;
        b = b.do_move(m, &si);
        pv.moves[pv.len++] = m;
    }
}

// Multiply a and b as though they are 128bit and return the high 64 bits
constexpr inline uint64_t mul_hi64(uint64_t a, uint64_t b) {
#if defined(__GNUC__) && defined(IS_64BIT)
    __extension__ using uint128 = unsigned __int128;
    return (uint128(a) * uint128(b)) >> 64;
#else
    uint64_t aL = uint32_t(a), aH = a >> 32;
    uint64_t bL = uint32_t(b), bH = b >> 32;
    uint64_t c1 = (aL * bL) >> 32;
    uint64_t c2 = aH * bL + c1;
    uint64_t c3 = aL * bH + uint32_t(c2);
    return aH * bH + (c2 >> 32) + (c3 >> 32);
#endif
}

uint64_t TranspositionTable::bucket_by_key(uint64_t key) const {
    // A cool idea from Stockfish to use upper bits of the key as the bucket index.
    // 0 <= key/2^64 < 1, so 0 <= key/2^64 * size_ < size
    return mul_hi64(key, size_);
}


