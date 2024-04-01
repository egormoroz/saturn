#ifndef FTSET_HPP
#define FTSET_HPP

#include "../primitives/common.hpp"


namespace mini {

constexpr uint16_t index(Color pov, Square sq, Piece p) {
    const uint16_t p_idx = 2 * (type_of(p) - 1) + uint16_t(color_of(p) != pov);
    const uint16_t o_sq = sq ^ (pov == WHITE ? 0 : 56);
    return p_idx * 64 + o_sq;
}

constexpr int N_FEATURES = 12 * 64;


} // mini


#endif
