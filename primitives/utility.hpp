#ifndef PRIMITIVE_UTILITY_HPP
#define PRIMITIVE_UTILITY_HPP

#include <string_view>
#include <iosfwd>
#include "common.hpp"
#include "bitboard.hpp"

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
