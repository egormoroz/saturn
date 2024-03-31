#ifndef BOOK_HPP
#define BOOK_HPP

#include "primitives/common.hpp"
#include <cstddef>
#include <vector>

; // this mysterious semicolon fixes clangd warning bug
#pragma pack(push, 1)

struct BookEntry {
    uint64_t key;
    Move move;
    uint16_t weight;
};

#pragma pack(pop)

class Book {
public:
    Book() = default;

    Move probe(uint64_t key, bool random = true) const;

    bool load_from_fens(const char* fens_file, bool assume_sorted = false);
    bool load_from_bin(const char* bin_path);

    bool save_to_bin(const char* bin_path) const;

private:
    size_t pick_from_group(size_t entry_idx, bool random) const;

    std::vector<BookEntry> entries_;
};

#endif
