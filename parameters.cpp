#include "parameters.hpp"


namespace params {

namespace defaults {

const int tt_size = 256;

const int asp_init_delta = 22;
const int asp_min_depth = 7;

const float lmr_coeff = 0.65f;

const int move_overhead = 30;


#ifdef EVALFILE
const char* nnue_weights_path = EVALFILE;
#else
const char* nnue_weights_path = "mini.nnue";
#endif


}

int asp_init_delta = defaults::asp_init_delta;
int asp_min_depth = defaults::asp_min_depth;

float lmr_coeff = defaults::lmr_coeff;

int move_overhead = defaults::move_overhead;

}

