#pragma once

#include <cstring>
#include <istream>

#include "linear.hpp"
#include "crelu.hpp"
#include "transformer.hpp"

constexpr int LOG2_WEIGHT_SCALE = 6;
// wtf is this bullshit?
/* constexpr int FV_SCALE = 16; */
constexpr int FV_SCALE = 64;

using TransformerLayer = FeatureTransformer<
    nnspecs::HALFKP_FEATURE_NB, nnspecs::HALFKP>;

struct Network {
    bool load_parameters(std::istream &is) {
        return l1.load_parameters(is) 
            && l2.load_parameters(is)
            && output.load_parameters(is);
    }

    int32_t propagate(const int8_t *input) const {
        struct alignas(SIMD_ALIGN) Buffer {
            Buffer() {
                memset(this, 0, sizeof(*this));
            }

            alignas(SIMD_ALIGN) L1::OutputBuffer l1_out;

            alignas(SIMD_ALIGN) L2::InputBuffer l2_in;
            alignas(SIMD_ALIGN) L2::OutputBuffer l2_out;

            alignas(SIMD_ALIGN) Lout::InputBuffer out_in;
        };

        alignas(SIMD_ALIGN) static thread_local Buffer buffer;
        
        l1.forward(input, buffer.l1_out);
        scale_and_clamp<
            L1::n_out_aligned,
            LOG2_WEIGHT_SCALE
        >(buffer.l1_out, buffer.l2_in);

        l2.forward(buffer.l2_in, buffer.l2_out);
        scale_and_clamp<
            L2::n_out_aligned,
            LOG2_WEIGHT_SCALE
        >(buffer.l2_out, buffer.out_in);

        int32_t result;
        output.forward(buffer.out_in, &result);
        return result / FV_SCALE;
    }

    using L1 = LinearLayer<nnspecs::L1_IN, nnspecs::L1_OUT>;
    using L2 = LinearLayer<nnspecs::L2_IN, nnspecs::L2_OUT>;
    using Lout = LinearLayer<nnspecs::L3_IN, 1>;

    alignas(SIMD_ALIGN) L1 l1;
    alignas(SIMD_ALIGN) L2 l2;
    alignas(SIMD_ALIGN) Lout output;
};

