#pragma once

#include "../primitives/common.hpp"

class Board;
struct Accumulator;
struct StateInfo;

namespace nnue {

bool load_parameters(const char *path);

bool update_accumulator(
        StateInfo *si, 
        Color perspective,
        Square ksq);

void refresh_accumulator(
        const Board &b, 
        Accumulator &acc,
        Color perspective);

int32_t evaluate(const Board &b);

} //nnue

