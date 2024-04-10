#ifndef PARAMETERS_HPP
#define PARAMETERS_HPP

#ifndef EVALFILE
#define EVALFILE "default.nnue"
#endif


namespace params {

namespace defaults {

extern const int tt_size;

extern const int move_overhead;

extern const char* nnue_weights_path;

}

struct Parameter {
    Parameter() = default;
    Parameter(const char* name, int def, int min, int max, int step)
        : name(name), def(def), min(min), max(max), step(step), val(def)
    {}

    operator int() const { return val; }

    const char* name;
    int def, min, max, step;
    int val;
};

struct Registry {
    Parameter& add(Parameter p);

    Parameter params[64];
    int n_params = 0;
};

extern Registry registry;


// Tunable parameters

extern Parameter& asp_init_delta;
extern Parameter& asp_min_depth;
extern Parameter& lmr_coeff;

extern Parameter& nmp_min_depth;
extern Parameter& nmp_base;
extern Parameter& nmp_depth_div;
extern Parameter& nmp_eval_div;

extern Parameter& iir_min_depth;

extern Parameter& rfp_max_depth;
extern Parameter& rfp_margin;

extern Parameter& rz_max_depth;
extern Parameter& rz_margin;

extern Parameter& sing_min_depth;

extern Parameter& lmr_hist_div;

extern Parameter& delta_margin;

extern Parameter& seefp_depth;


// Not tunable parameters

extern int move_overhead;

}

#endif
