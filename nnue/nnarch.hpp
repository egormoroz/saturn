#pragma once

#include <cstddef>

using std::size_t;

namespace nnspecs {

constexpr size_t HALFKP_FEATURE_NB = 40960;
//constexpr size_t HALFKP_FEATURE_NB = 41024;

constexpr size_t HALFKP = 256;

constexpr size_t L1_IN = 512;
constexpr size_t L1_OUT = 32;

constexpr size_t L2_IN = 32;
constexpr size_t L2_OUT = 32;

constexpr size_t L3_IN = 32;

}


