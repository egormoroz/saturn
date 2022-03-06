#ifndef EVAL_HPP
#define EVAL_HPP

#include "../primitives/common.hpp"
#include "../primitives/bitboard.hpp"

class Board;

bool is_endgame(const Board &b);

int count_material(const Board &b, Bitboard mask, Color side);

int eval(const Board &b);

#endif
