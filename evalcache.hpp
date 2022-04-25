#pragma once

#include <cstdint>
#include <cstring>

struct EvalCache {
    static constexpr size_t KEY_SIZE = 16;
    static constexpr size_t MASK = 0xFFFF;
    static constexpr size_t SIZE = size_t(1) << KEY_SIZE;

    EvalCache() {
        memset(table_, 0, sizeof(table_));
    }

    void store(uint64_t pos_hash, int16_t eval) {
        table_[pos_hash & MASK] = 
            (pos_hash & ~0xFFFF) | uint16_t(eval);
    }

    bool probe(uint64_t pos_hash, int16_t &eval) const {
        uint64_t e = table_[pos_hash & MASK];
        uint64_t key = (e & ~0xFFFF) | (pos_hash & 0xFFFF);

        eval = int16_t(uint16_t(e & 0xFFFF));
        return key == pos_hash;
    }

    uint64_t table_[SIZE];
};

