#ifndef OPS_HPP
#define OPS_HPP

#include <cstdint>
#include "simd.hpp"

namespace nnue {

template<size_t N, int shift = 0>
void scale_and_clamp(const int16_t* RESTRICT input, int8_t* RESTRICT output) {
    constexpr size_t in_register_width = simd_reg_width / 16;
    constexpr size_t out_register_width = simd_reg_width / 8;
    static_assert(N % out_register_width == 0);
    constexpr size_t num_out_chunks = N / out_register_width;
    
    const __m256i zero = _mm256_setzero_si256();
    constexpr int control = 0b11011000;

    for (size_t i = 0; i < num_out_chunks; ++i) {
        const size_t offset0 = (i * 2 + 0) * in_register_width;
        const size_t offset1 = (i * 2 + 1) * in_register_width;

        __m256i in0 = m256_load(&input[offset0]);
        __m256i in1 = m256_load(&input[offset1]);

        if constexpr (shift) {
            in0 = _mm256_srai_epi16(in0, shift);
            in1 = _mm256_srai_epi16(in1, shift);
        }

        const __m256i result = 
            _mm256_permute4x64_epi64(
                _mm256_max_epi8(
                    _mm256_packs_epi16(in0, in1),
                    zero
                ),
                control
            );
        
        _mm256_store_si256((__m256i*)&output[i * out_register_width], result);
    }
}

template<size_t N, int shift>
void scale_and_clamp(const int32_t* RESTRICT input, int8_t* RESTRICT output) {
    constexpr size_t in_register_width = 256 / 32;
    constexpr size_t out_register_width = 256 / 8;
    static_assert(N % out_register_width == 0);
    constexpr size_t num_out_chunks = N / out_register_width;

    const __m256i zero = _mm256_setzero_si256();
    const __m256i control = _mm256_set_epi32(7, 3, 6, 2, 5, 1, 4, 0);

    for (size_t i = 0; i < num_out_chunks; ++i) {
        const size_t offset0 = (i * 4 + 0) * in_register_width;
        const size_t offset1 = (i * 4 + 1) * in_register_width;
        const size_t offset2 = (i * 4 + 2) * in_register_width;
        const size_t offset3 = (i * 4 + 3) * in_register_width;

        __m256i in0 = _mm256_packs_epi32(
            m256_load(&input[offset0]),
            m256_load(&input[offset1])
        );
        __m256i in1 = _mm256_packs_epi32(
            m256_load(&input[offset2]),
            m256_load(&input[offset3])
        );

        in0 = _mm256_srai_epi16(in0, shift);
        in1 = _mm256_srai_epi16(in1, shift);

        const __m256i result = 
            _mm256_permutevar8x32_epi32(
                _mm256_max_epi8(
                    _mm256_packs_epi16(in0, in1),
                    zero
                ),
                control
            );

        _mm256_store_si256((__m256i*)&output[i * out_register_width], result);
    }
}

namespace detail {

constexpr int N_OUTPUT_REGS = 8;   // how many registers to use for accumulators
constexpr int SIMD_VEC_WIDTH = 32; // in bytes


using acc_vec_t = __m256i;
using bias_vec_t = __m128i;
using weight_vec_t = __m256i;
using in_vec_t = __m256i;

template<size_t N_INPUT, size_t N_OUTPUT>
struct LargeLayout {
    static constexpr size_t small_block_size = SIMD_VEC_WIDTH;
    static constexpr size_t big_block_size = N_OUTPUT_REGS * N_INPUT;
    static constexpr size_t n_small_blocks_in_big_block = big_block_size / small_block_size;
    static constexpr size_t n_small_blocks_per_output = N_INPUT / small_block_size;
    static constexpr size_t n_big_blocks = N_OUTPUT / N_OUTPUT_REGS;
};

template<size_t N_INPUT, size_t N_OUTPUT>
void linear_large_permute(
        const int8_t*  RESTRICT weight_in,
              int8_t*  RESTRICT weight_out)
{
    using L = LargeLayout<N_INPUT, N_OUTPUT>;
    for (size_t i = 0; i < N_INPUT * N_OUTPUT; ++i) {
        const size_t small_block = (i / L::small_block_size) % L::n_small_blocks_in_big_block;
        const size_t small_block_col = small_block / L::n_small_blocks_per_output;
        const size_t small_block_row = small_block % L::n_small_blocks_per_output;
        const size_t big_block = i / L::big_block_size;
        const size_t rest = i % L::small_block_size;

        const size_t idx = 
              big_block * L::big_block_size
            + small_block_row * L::small_block_size * N_OUTPUT_REGS
            + small_block_col * L::small_block_size
            + rest;

        weight_out[idx] = weight_in[i];
    }
}

template<size_t N_INPUT, size_t N_OUTPUT>
void linear_forward_large(
        const int8_t*  RESTRICT x, 
              int32_t* RESTRICT out,
        const int8_t*  RESTRICT weight, 
        const int32_t* RESTRICT bias) 
{
    const in_vec_t *invec = reinterpret_cast<const in_vec_t*>(x);
    
    using L = LargeLayout<N_INPUT, N_OUTPUT>;

    for (size_t big_block = 0; big_block < L::n_big_blocks; ++big_block) {
        acc_vec_t acc[N_OUTPUT_REGS]{};

        for (size_t small_block = 0; small_block < L::n_small_blocks_per_output; 
                small_block += 2)
        {
            const weight_vec_t *weightvec = 
                reinterpret_cast<const weight_vec_t*>(
                      weight
                    + big_block * L::big_block_size
                    + small_block * L::small_block_size * N_OUTPUT_REGS
                );

            const in_vec_t in0 = invec[small_block + 0];
            const in_vec_t in1 = invec[small_block + 1];

            for (size_t k = 0; k < N_OUTPUT_REGS; ++k)
                m256_add_dpbusd_epi32x2(acc[k], in0, weightvec[k],
                    in1, weightvec[k + N_OUTPUT_REGS]);
        }

        bias_vec_t *outputvec = reinterpret_cast<bias_vec_t*>(out);
        const bias_vec_t *biasvec = reinterpret_cast< const bias_vec_t*>(bias);

        for (size_t k = 0; k < N_OUTPUT_REGS; k += 4) {
            const size_t idx = (big_block * N_OUTPUT_REGS + k) / 4;
            outputvec[idx] = m256_haddx4(acc[k+0], acc[k+1],
                    acc[k+2], acc[k+3], biasvec[idx]);
        }
    }
}

template<size_t N_INPUT, size_t N_OUTPUT>
void linear_forward_medium(
        const int8_t*  RESTRICT x, 
              int32_t* RESTRICT out,
        const int8_t*  RESTRICT weight, 
        const int32_t* RESTRICT bias) 
{
    static_assert(N_OUTPUT % N_OUTPUT_REGS == 0);
    static_assert(N_INPUT % SIMD_VEC_WIDTH == 0);

    const in_vec_t *input_vec = (const in_vec_t*)x;
    const bias_vec_t *bias_vec = (const bias_vec_t*)bias;
    bias_vec_t *out_vec = (bias_vec_t*)out;

    constexpr int n_out_blocks = N_OUTPUT / N_OUTPUT_REGS;
    constexpr int n_in_blocks = N_INPUT / SIMD_VEC_WIDTH;

    constexpr int bias_vec_len = sizeof(bias_vec_t) / 4;

    for (int i = 0; i < n_out_blocks; ++i) {
        acc_vec_t acc[N_OUTPUT_REGS]{};

        for (int j = 0; j < n_in_blocks; ++j) {
            const in_vec_t in_vec = input_vec[j];

            for (int k = 0; k < N_OUTPUT_REGS; ++k) {
                const int row_offset = (i * N_OUTPUT_REGS + k) * N_INPUT;
                auto weight_slice = (const weight_vec_t*)&weight[row_offset];
                m256_add_dpbusd_epi32(acc[k], in_vec, weight_slice[j]);
            }
        }


        for (int k = 0; k < N_OUTPUT_REGS; k += 4) {
            int idx = i * (N_OUTPUT_REGS / bias_vec_len) + k / 4;
            out_vec[idx] = m256_haddx4(acc[k+0], acc[k+1], acc[k+2], acc[k+3], bias_vec[idx]);
        }
    }
}

template<size_t N_INPUT>
void linear_forward_single(
        const int8_t*  RESTRICT x, 
              int32_t* RESTRICT out,
        const int8_t*  RESTRICT weight, 
        const int32_t* RESTRICT bias) 
{
    out[0] = bias[0];
    for (size_t i = 0; i < N_INPUT; ++i)
        out[0] += static_cast<int32_t>(weight[i]) * x[i];
}

} // detail


template<size_t N_INPUT, size_t N_OUTPUT>
void linear_forward(
        const int8_t*  RESTRICT x, 
              int32_t* RESTRICT out,
        const int8_t*  RESTRICT weight, 
        const int32_t* RESTRICT bias) 
{
    if constexpr (N_INPUT >= 128)
        detail::linear_forward_large<N_INPUT, N_OUTPUT>(x, out, weight, bias);
    else if constexpr (N_OUTPUT > 1)
        detail::linear_forward_medium<N_INPUT, N_OUTPUT>(x, out, weight, bias);
    else
        detail::linear_forward_single<N_INPUT>(x, out, weight, bias);
}

} // nnue


#endif
