#ifndef PRIMITIVE_UTILITY_HPP
#define PRIMITIVE_UTILITY_HPP

#include <string_view>
#include <iosfwd>
#include "common.hpp"
#include "bitboard.hpp"

constexpr bool is_upper(char ch) {
    return ch >= 'A' && ch <= 'Z';
}

constexpr bool is_lower(char ch) {
    return ch >= 'a' && ch <= 'z';
}

constexpr bool is_digit(char ch) {
    return ch >= '0' && ch <= '9';
}

constexpr char to_upper(char ch) {
    return is_lower(ch) ? ch - ('a' - 'A') : ch;
}

constexpr char to_lower(char ch) {
    return is_upper(ch) ? ch + ('a' - 'A') : ch;
}

inline void trim_front(std::string_view &sv) {
    size_t i = 0;
    for (; i < sv.size() && sv[i] == ' '; ++i);
    sv = sv.substr(i);
}

inline void skip_word(std::string_view &sv) {
    size_t i = 0;
    for (; i < sv.size() && sv[i] != ' '; ++i);
    sv = sv.substr(i);
}

inline void next_word(std::string_view &sv) {
    size_t i = 0;
    for (; i < sv.size() && sv[i] == ' '; ++i);
    for (; i < sv.size() && sv[i] != ' '; ++i);
    for (; i < sv.size() && sv[i] == ' '; ++i);
    sv = sv.substr(i);
}

//checks if strings are equal, ignoring case
inline bool istr_equal(std::string_view a, std::string_view b) {
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (to_lower(a[i]) != to_lower(b[i]))
            return false;
    return true;
}

struct BBPretty {
    Bitboard bb;
};

struct Score {
    int value;
};

class Board;

Square square_from_str(std::string_view sv);
Color color_from_str(std::string_view sv);
Piece piece_from_str(std::string_view sv);
PieceType ptype_from_str(std::string_view sv);
CastlingRights castling_from_str(std::string_view sv);

//extracts only from/to squares and promotion type
Move move_from_str(std::string_view sv);

//extracts move and tests if it is valid
Move move_from_str(const Board &b, std::string_view sv);

File file_from_ch(char ch);
Rank rank_from_ch(char ch);

std::ostream& operator<<(std::ostream& os, Square s);
std::ostream& operator<<(std::ostream& os, Color c);
std::ostream& operator<<(std::ostream& os, PieceType pt);
std::ostream& operator<<(std::ostream& os, Piece p);
std::ostream& operator<<(std::ostream& os, CastlingRights cr);
std::ostream& operator<<(std::ostream& os, Move m);
std::ostream& operator<<(std::ostream& os, BBPretty b);
std::ostream& operator<<(std::ostream& os, Score s);


#endif
