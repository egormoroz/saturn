#ifndef EVAL_HPP
#define EVAL_HPP

#include "../primitives/common.hpp"
#include "../primitives/bitboard.hpp"

class Board;

void init_ps_tables();

int eval(const Board &b);

#endif
