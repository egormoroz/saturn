#ifndef EVAL_HPP
#define EVAL_HPP

#include "../primitives/common.hpp"

constexpr int mg_value[PIECE_TYPE_NB] = { 0, 82, 337, 365, 477, 1025,  0};
constexpr int eg_value[PIECE_TYPE_NB] = { 0, 94, 281, 297, 512,  936,  0};

constexpr int ENDGAME_MAT = mg_value[QUEEN] + mg_value[BISHOP];

class Board;

void init_ps_tables();
int16_t evaluate(const Board &b);

#endif
