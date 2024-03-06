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

    BOUND_NUM = 4,
};

// TODO: store evaluation score as well
struct TTEntry {
    uint64_t key;
    union {
        uint64_t data;
        struct {
            uint16_t move16;
            int16_t score16;
            int16_t eval16;

            uint8_t depth5 : 5;
            uint8_t bound2: 2;
            uint8_t avoid_null: 1;

            uint8_t age;
        };
    };

    int score(int ply) const;

    TTEntry() = default;
    TTEntry(uint64_t key, int score, int eval, Bound b, int depth, 
            Move m, int ply, bool avoid_null);
};

struct PVLine {
    static constexpr int MAX_LEN = MAX_DEPTH;

    Move moves[MAX_LEN];
    int score;
    int len=0;
};

class TranspositionTable {
    struct Bucket {
        static constexpr int N = 4;
        TTEntry entries[N];
    };
public:
    TranspositionTable() = default;

    void resize(size_t mbs);
    void clear();

    void new_search();

    bool probe(uint64_t key, TTEntry &e) const;
    void store(TTEntry entry);

    int extract_pv(Board b, Move *pv, int len);

    void extract_pv(Board b, PVLine &pv, int max_len, 
            Move first_move = MOVE_NONE);

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
