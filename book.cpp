#include "book.hpp"
#include "board/board.hpp"
#include "primitives/utility.hpp"
#include <fstream>
#include <string>
#include <algorithm>
#include <random>


Move Book::probe(uint64_t key, bool random) const {
    size_t lo = 0, hi = entries_.size();

    while (lo <= hi) {
        size_t mid = (lo + hi) / 2;
        const BookEntry &e = entries_[mid];

        if (key < e.key) {
            hi = mid - 1;
        } else if (key > e.key) {
            lo = mid + 1;
        } else {
            return entries_[pick_from_group(mid, random)].move;
        }
    }

    return MOVE_NONE;
}

bool Book::load_from_fens(const char* fens_file, bool assume_sorted) {
    std::ifstream fin(fens_file);
    if (!fin)
        return false;

    BookEntry e;
    std::string s;
    Board b;
    while (fin >> s) {
        e.move = move_from_str(s);
        if (!(fin >> e.weight)) return false;
        if (!std::getline(fin, s)) return false;
        if (!b.load_fen(s)) return false;
        e.key = b.key();
        entries_.push_back(e);
    }

    if (!assume_sorted) {
        std::sort(entries_.begin(), entries_.end(), 
                [](auto &x, auto &y) { return x.weight > y.weight; });
        std::stable_sort(entries_.begin(), entries_.end(), 
                [](auto &x, auto &y) { return x.key < y.key; });
    }

    return true;
}

bool Book::load_from_bin(const char* bin_path) {
    std::ifstream fin(bin_path, std::ios::binary);
    if (!fin) return false;

    fin.seekg(0, std::ios::end);
    size_t size = fin.tellg();
    fin.clear();
    fin.seekg(0);

    if (size % sizeof(BookEntry) != 0) return false;
    entries_.resize(size / sizeof(BookEntry));

    return !!fin.read((char*)entries_.data(), size);
}

bool Book::save_to_bin(const char* bin_path) const {
    std::ofstream fout(bin_path, std::ios::binary);
    if (!fout) return false;

    return !!fout.write((const char*)entries_.data(),
            entries_.size() * sizeof(BookEntry));

}

size_t Book::pick_from_group(size_t entry_idx, bool random) const {
    const uint64_t key = entries_[entry_idx].key;
    static thread_local std::default_random_engine rng(0xdeadbeef);

    size_t start = entry_idx;
    while (start > 0 && entries_[start - 1].key == key)
        --start;

    if (!random) return start;

    size_t end = entry_idx + 1;
    while (end < entries_.size() && entries_[end].key == key)
        ++end;

    int32_t sum = 0;
    for (size_t i = start; i < end; ++i)
        sum += entries_[i].weight;


    std::uniform_int_distribution<int32_t> dist(0, sum - 1);
    int32_t x = dist(rng);

    for (size_t i = start; i < end; ++i) {
        if (x < (int32_t)entries_[i].weight)
            return i;
        x -= entries_[i].weight;
    }

    // unreachable
    assert(false);
    return ~size_t(0);
}

