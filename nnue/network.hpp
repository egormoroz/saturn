#pragma once

#include <cstring>
#include <istream>

#include "linear.hpp"
#include "crelu.hpp"
#include "transformer.hpp"

constexpr int LOG2_WEIGHT_SCALE = 6;

using TransformerLayer = FeatureTransformer<
    nnspecs::HALFKP_FEATURE_NB, nnspecs::HALFKP>;

struct Network {
    bool load_parameters(std::istream &is) {
        return l1.load_parameters(is) 
            && l2.load_parameters(is)
            && output.load_parameters(is);
    }

    int32_t propagate(const int8_t *input) const {
        struct alignas(64) Buffer {
            Buffer() {
                memset(this, 0, sizeof(*this));
            }

            alignas(64) decltype(l1)::OutputBuffer l1_out;

            alignas(64) decltype(l2)::InputBuffer l2_in;
            alignas(64) decltype(l2)::OutputBuffer l2_out;

            alignas(64) decltype(output)::InputBuffer out_in;
        };

        alignas(64) static thread_local Buffer buffer;
        
        l1.propagate(input, buffer.l1_out);
        scale_and_clamp<
            l1.OUT_DIM_ALIGNED,
            LOG2_WEIGHT_SCALE
        >(buffer.l1_out, buffer.l2_in);

        l2.propagate(buffer.l2_in, buffer.l2_out);
        scale_and_clamp<
            l2.OUT_DIM_ALIGNED,
            LOG2_WEIGHT_SCALE
        >(buffer.l2_out, buffer.out_in);

        int32_t result;
        output.propagate(buffer.out_in, &result);
        return result >> LOG2_WEIGHT_SCALE;
    }

    LinearLayer<nnspecs::L1_IN, nnspecs::L1_OUT> l1;
    LinearLayer<nnspecs::L2_IN, nnspecs::L2_OUT> l2;
    LinearLayer<nnspecs::L3_IN, 1> output;
};

