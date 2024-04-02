#ifndef NNUE_HPP
#define NNUE_HPP

#include "../primitives/common.hpp"
#include "state.hpp"

class Board;

namespace mini {

bool update_accumulator( StateInfo *si, Color pov, Square ksq);
void refresh_accumulator(const Board &b, Accumulator &acc, Color pov);

int32_t evaluate(const Board &b);
bool load_parameters(const char *path);


} // mini


using StateInfo = mini::StateInfo;

#endif
