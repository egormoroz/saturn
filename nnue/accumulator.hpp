#pragma once

#include <cstdint>
#include "nnarch.hpp"
#include "simd.hpp"

struct Accumulator {
    alignas(SIMD_ALIGN) int16_t v[2][nnspecs::HALFKP];
    bool computed[2];
};

