#ifndef TT_HPP
#define TT_HPP

#include "primitives/common.hpp"
#include <cstddef>

class Board;

enum Bound {
    BOUND_NONE = 0,
    BOUND_ALPHA = 1,
    BOUND_BETA = 2,
    BOUND_EXACT = 3,
};

struct TTEntry {
    uint64_t key;
    union {
        uint64_t data;
        struct {
            uint16_t move16;
            int16_t score16;
            uint8_t depth8;
            uint8_t bound8;
            bool avoid_null;
            uint8_t age;
        };
    };

    TTEntry() = default;
    TTEntry(uint64_t key, int score, Bound b, int depth, Move m,
            bool avoid_null);
};

enum ProbeResult {
    HASH_HIT,
    HASH_MISS,
};

class TranspositionTable {
    struct Bucket {
        static constexpr int N = 4;
        TTEntry entries[N];
    };
public:
    TranspositionTable() = default;

    void resize(size_t mbs);

    void new_search();

    ProbeResult probe(uint64_t key, TTEntry &e) const;
    void store(TTEntry entry);

    int extract_pv(Board b, Move *pv, int len);

    void prefetch(uint64_t key) const;

    uint64_t hashfull() const;

    ~TranspositionTable();

private:
    Bucket* buckets_{};
    size_t size_{};
    uint8_t age_{};
};

extern TranspositionTable g_tt;

#endif
