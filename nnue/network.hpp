#pragma once

#include <cstring>
#include <istream>

#include "transformer.hpp"
#include "ops.hpp"

namespace nnue {

constexpr int LOG2_WEIGHT_SCALE = 6;

using TransformerLayer = FeatureTransformer<
    nnspecs::HALFKP_FEATURE_NB, nnspecs::HALFKP>;

namespace detail {

template<size_t N_INPUT, size_t N_OUTPUT>
struct Linear {
    static constexpr size_t N_IN = N_INPUT;
    static constexpr size_t N_OUT = N_OUTPUT;

    using InputBuffer = int8_t[N_IN];
    using OutputBuffer = int32_t[N_OUT];

    bool load_parameters(std::istream &is) {
        if (!is.read((char*)bias, sizeof(bias)))
            return false;

        if constexpr (N_IN >= 128) {
            int8_t buf[N_IN * N_OUT];
            if (!is.read((char*)buf, sizeof(buf)))
                return false;

            detail::linear_large_permute<N_IN, N_OUT>(buf, weight);
            return true;
        }

        if (!is.read((char*)weight, sizeof(weight)))
            return false;

        return true;
    }


    void forward(const int8_t*  RESTRICT x, int32_t* RESTRICT out) const {
        linear_forward<N_IN, N_OUT>(x, out, weight, bias);
    }

    alignas(SIMD_ALIGN) int8_t  weight[N_IN * N_OUT];
    alignas(SIMD_ALIGN) int32_t bias[N_OUT];
};

}

struct Network {
    bool load_parameters(std::istream &is) {
        return l1.load_parameters(is) 
            && l2.load_parameters(is)
            && output.load_parameters(is);
    }

    int32_t forward(const int8_t *input) const {
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
        scale_and_clamp<L1::N_OUT, LOG2_WEIGHT_SCALE >(buffer.l1_out, buffer.l2_in);


        l2.forward(buffer.l2_in, buffer.l2_out);
        scale_and_clamp<L2::N_OUT, LOG2_WEIGHT_SCALE >(buffer.l2_out, buffer.out_in);

        int32_t result;
        output.forward(buffer.out_in, &result);
        return result >> LOG2_WEIGHT_SCALE;
    }

    using L1 = detail::Linear<nnspecs::L1_IN, nnspecs::L1_OUT>;
    using L2 = detail::Linear<nnspecs::L2_IN, nnspecs::L2_OUT>;
    using Lout = detail::Linear<nnspecs::L3_IN, 1>;

    alignas(SIMD_ALIGN) int8_t  l1_weight[nnspecs::L1_IN * nnspecs::L1_OUT];
    alignas(SIMD_ALIGN) int32_t l1_bias[nnspecs::L1_OUT];

    alignas(SIMD_ALIGN) int8_t  l2_weight[nnspecs::L2_IN * nnspecs::L2_OUT];
    alignas(SIMD_ALIGN) int32_t l2_bias[nnspecs::L2_OUT];

    alignas(SIMD_ALIGN) int8_t  l3_weight[nnspecs::L3_IN * 1];
    alignas(SIMD_ALIGN) int32_t l3_bias[1];

    alignas(SIMD_ALIGN) L1 l1;
    alignas(SIMD_ALIGN) L2 l2;
    alignas(SIMD_ALIGN) Lout output;
};

}
