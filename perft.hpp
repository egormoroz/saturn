#ifndef PERFT_HPP
#define PERFT_HPP

#include <cstdint>

class Board;

uint64_t perft(const Board &b, int depth);
int perft_test_positions();

#endif
