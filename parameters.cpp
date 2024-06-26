#include "parameters.hpp"
#include "search/search.hpp"


namespace params {

Parameter& Registry::add(Parameter p) {
    params[n_params] = p;
    return params[n_params++];
}

Registry registry;


namespace defaults {

const int tt_size = 16;
const int move_overhead = 30;
const char* nnue_weights_path = EVALFILE;


}

int move_overhead = defaults::move_overhead;

#define PARAMETER(name, def, min, max, step) \
    Parameter& name = registry.add(Parameter(#name, def, min, max, step));

PARAMETER(asp_init_delta, 12, 1, 100, 5)
PARAMETER(asp_min_depth, 8, 1, 12, 1)
PARAMETER(lmr_coeff, 48, 40, 100, 5)


PARAMETER(nmp_min_depth, 3, 1, 10, 1)
PARAMETER(nmp_base, 4, 2, 4, 1)
PARAMETER(nmp_depth_div, 8, 4, 8, 1)
PARAMETER(nmp_eval_div, 116, 80, 200, 20)

PARAMETER(iir_min_depth, 2, 2, 8, 2)

PARAMETER(rfp_max_depth, 7, 2, 8, 2)
PARAMETER(rfp_margin, 119, 50, 250, 25)

PARAMETER(rz_max_depth, 6, 2, 8, 2)
PARAMETER(rz_margin, 239, 100, 300, 50)

PARAMETER(sing_min_depth, 8, 6, 12, 1)

PARAMETER(lmr_hist_div, 4903, 2048, 12288, 512)

PARAMETER(seefp_depth, 5, 2, 12, 1)

}

// All autoinit that uses the params must be in the same translation unit!

namespace {

struct LMRAutoInit {
    LMRAutoInit() {
        update_reduction_tables();
    }
} _;

}

