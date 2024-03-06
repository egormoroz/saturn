#pragma once

#include <cstdint>
#include "../config.hpp"
#include "nnarch.hpp"
#include "simd.hpp"

struct Accumulator {
#ifndef NONNUE
    alignas(SIMD_ALIGN) int16_t v[2][nnspecs::HALFKP];
#endif
    bool computed[2];
};

