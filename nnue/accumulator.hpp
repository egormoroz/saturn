#pragma once

#include <cstdint>
#include "nnarch.hpp"

struct Accumulator {
    int16_t v[2][nnspecs::HALFKP];
    int32_t psqt[2];
    bool computed[2];
};

