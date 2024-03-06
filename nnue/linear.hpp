#pragma once

#include <cstring>
#include <istream>
#include <type_traits>
#include "simd.hpp"
#include "nnue_misc.hpp"


template<size_t InputDims, size_t OutputDims, typename Enabled = void>
struct LinearLayer;


template<size_t N_INPUT, size_t N_OUTPUT>
struct LinearLayer<N_INPUT, N_OUTPUT, 
    std::enable_if_t<(N_INPUT >= 128)>> 
{
#if defined(USE_AVX2)
    static constexpr size_t simd_vec_width = 32;
    static constexpr size_t n_output_regs = 8;

    using acc_vec_t = __m256i;
    using bias_vec_t = __m128i;
    using weight_vec_t = __m256i;
    using in_vec_t = __m256i;

    #define vec_zero _mm256_setzero_si256()
    #define vec_haddx4 m256_haddx4
    #define vec_add_dpbusd_epi32x2 m256_add_dpbusd_epi32x2
    #define vec_add_dpbusd_epi32 m256_add_dpbusd_epi32

#elif defined(USE_SSSE3)
    static constexpr size_t simd_vec_width = 16;
    static constexpr size_t n_output_regs = 4;

    using acc_vec_t = __m128i;
    using bias_vec_t = __m128i;
    using weight_vec_t = __m128i;
    using in_vec_t = __m128i;

    #define vec_zero _mm_setzero_si128()
    #define vec_haddx4 m128_haddx4
    #define vec_add_dpbusd_epi32 m128_add_dpbusd_epi32
    #define vec_add_dpbusd_epi32x2 m128_add_dpbusd_epi32x2
#endif

    static_assert(N_INPUT % simd_vec_width == 0);
    static_assert(N_OUTPUT % (simd_vec_width / 4) == 0);

    static constexpr size_t small_block_size = simd_vec_width;
    static constexpr size_t big_block_size = n_output_regs * N_INPUT;
    static constexpr size_t n_small_blocks_in_big_block = big_block_size / small_block_size;
    static constexpr size_t n_small_blocks_per_output = N_INPUT / small_block_size;
    static constexpr size_t n_big_blocks = N_OUTPUT / n_output_regs;

    static constexpr size_t n_in_aligned = N_INPUT;
    static constexpr size_t n_out_aligned = N_OUTPUT;

    using InputBuffer = int8_t[n_in_aligned];
    using OutputBuffer = int32_t[n_out_aligned];

    static size_t get_weight_index(size_t i) {
        const size_t small_block = (i / small_block_size) % n_small_blocks_in_big_block;
        const size_t small_block_col = small_block / n_small_blocks_per_output;
        const size_t small_block_row = small_block % n_small_blocks_per_output;
        const size_t big_block = i / big_block_size;
        const size_t rest = i % small_block_size;

        const size_t idx = 
              big_block * big_block_size
            + small_block_row * small_block_size * n_output_regs
            + small_block_col * small_block_size
            + rest;
        return idx;
    }

    bool load_parameters(std::istream &is) {
        is.read((char*)bias_, 4 * N_OUTPUT);

        for (size_t i = 0; i < N_INPUT * N_OUTPUT; ++i)
            weight_[get_weight_index(i)] = is.get();

        return !is.fail();
    }

    void forward(const int8_t *input, int32_t *output) const {
        const in_vec_t *invec = reinterpret_cast<const in_vec_t*>(input);

        for (size_t big_block = 0; big_block < n_big_blocks; ++big_block) {
            acc_vec_t acc[n_output_regs] = { vec_zero };

            for (size_t small_block = 0; small_block < n_small_blocks_per_output; 
                    small_block += 2)
            {
                const weight_vec_t *weightvec = 
                    reinterpret_cast<const weight_vec_t*>(
                          weight_
                        + big_block * big_block_size
                        + small_block * small_block_size * n_output_regs
                    );

                const in_vec_t in0 = invec[small_block + 0];
                const in_vec_t in1 = invec[small_block + 1];

                for (size_t k = 0; k < n_output_regs; ++k)
                    vec_add_dpbusd_epi32x2(acc[k], in0, weightvec[k],
                        in1, weightvec[k + n_output_regs]);
            }

            static_assert(n_output_regs % 4 == 0);
            bias_vec_t *outputvec = reinterpret_cast<bias_vec_t*>(output);
            const bias_vec_t *biasvec = 
                reinterpret_cast< const bias_vec_t*>(bias_);

            for (size_t k = 0; k < n_output_regs; k += 4) {
                const size_t idx = (big_block * n_output_regs + k) / 4;
                outputvec[idx] = vec_haddx4(acc[k+0], acc[k+1],
                        acc[k+2], acc[k+3], biasvec[idx]);
            }
        }
    }

    #undef vec_zero
    #undef vec_haddx4
    #undef vec_add_dpbusd_epi32
    #undef vec_add_dpbusd_epi32x2

private:
    alignas(SIMD_ALIGN) int8_t weight_[N_INPUT * N_OUTPUT];
    alignas(SIMD_ALIGN) int32_t bias_[N_OUTPUT];
};


template<size_t N_INPUT, size_t N_OUTPUT>
struct LinearLayer<N_INPUT, N_OUTPUT, 
    std::enable_if_t<(
        round_up<size_t>(N_INPUT, 32) < 128
        && N_OUTPUT >= 4
    )>>
{
#if defined(USE_AVX2)
    static constexpr size_t simd_vec_width = 32;
    static constexpr size_t n_output_regs = 8;

    using acc_vec_t = __m256i;
    using bias_vec_t = __m128i;
    using weight_vec_t = __m256i;
    using in_vec_t = __m256i;

    #define vec_zero _mm256_setzero_si256()
    #define vec_haddx4 m256_haddx4
    #define vec_add_dpbusd_epi32x2 m256_add_dpbusd_epi32x2
    #define vec_add_dpbusd_epi32 m256_add_dpbusd_epi32

#elif defined(USE_SSSE3)
    static constexpr size_t simd_vec_width = 16;
    static constexpr size_t n_output_regs = 4;

    using acc_vec_t = __m128i;
    using bias_vec_t = __m128i;
    using weight_vec_t = __m128i;
    using in_vec_t = __m128i;

    #define vec_zero _mm_setzero_si128()
    #define vec_haddx4 m128_haddx4
    #define vec_add_dpbusd_epi32 m128_add_dpbusd_epi32
    #define vec_add_dpbusd_epi32x2 m128_add_dpbusd_epi32x2
#endif

    static constexpr size_t n_in = N_INPUT;
    static constexpr size_t n_out = N_OUTPUT;

    static_assert(N_OUTPUT % 4 == 0, "out_dim must be aligned");

    static constexpr size_t n_in_aligned = 
        round_up<size_t>(n_in, simd_vec_width);
    static constexpr size_t n_out_aligned =
        round_up<size_t>(n_out, simd_vec_width);

    static_assert(n_in_aligned < 128);

    using InputBuffer = int8_t[n_in_aligned];
    using OutputBuffer = int32_t[n_out_aligned];

    bool load_parameters(std::istream &is) {
        constexpr size_t bias_bytes = n_out * 4;

        memset(bias_, 0, sizeof(bias_));
        if (!is.read((char*)bias_, bias_bytes))
            return false;

        memset(weight_, 0, sizeof(weight_));
        for (size_t i = 0; i < n_out; ++i) {
            size_t offset = n_in_aligned * i;
            if (!is.read((char*)&weight_[offset], n_in))
                return false;
        }

        return true;
    }

    void forward(const int8_t *input, int32_t *output) const {
        const in_vec_t *input_vec = (const in_vec_t*)input;
        const bias_vec_t *bias_vec = (const bias_vec_t*)bias_;
        bias_vec_t *out_vec = (bias_vec_t*)output;

        constexpr int n_out_blocks = n_out / n_output_regs;
        constexpr int n_in_blocks = n_in / simd_vec_width;

        constexpr int bias_vec_len = sizeof(bias_vec_t) / 4;

        static_assert(n_in % n_output_regs == 0);
        static_assert(n_in % simd_vec_width == 0);

        for (int i = 0; i < n_out_blocks; ++i) {
            acc_vec_t acc[n_output_regs] = { vec_zero };

            for (int j = 0; j < n_in_blocks; ++j) {
                const in_vec_t in_vec = input_vec[j];

                for (int k = 0; k < n_output_regs; ++k) {
                    const int row_offset = (i * n_output_regs + k) * n_in;
                    auto weight_slice = (const weight_vec_t*)&weight_[row_offset];
                    vec_add_dpbusd_epi32(acc[k], in_vec, weight_slice[j]);
                }
            }


            for (int k = 0; k < n_output_regs; k += 4) {
                int idx = i * (n_output_regs / bias_vec_len) + k / 4;
                out_vec[idx] = vec_haddx4(acc[k+0], acc[k+1], acc[k+2], acc[k+3], bias_vec[idx]);
            }
        }
    }

    #undef vec_zero
    #undef vec_haddx4
    #undef vec_add_dpbusd_epi32
    #undef vec_add_dpbusd_epi32x2

private:
    alignas(SIMD_ALIGN) int8_t weight_[n_in_aligned * n_out];
    alignas(SIMD_ALIGN) int32_t bias_[n_out];
};

template<size_t IN_DIM>
struct LinearLayer<IN_DIM, 1, void> {
    static constexpr size_t INPUT_DIM = IN_DIM;
    static constexpr size_t OUTPUT_DIM = 1;

    using InputBuffer = int8_t[INPUT_DIM];

    bool load_parameters(std::istream &is) {
        constexpr size_t bias_bytes = OUTPUT_DIM * 4;
        constexpr size_t weight_bytes = INPUT_DIM * OUTPUT_DIM;

        memset(bias_, 0, sizeof(bias_));
        memset(weight_, 0, sizeof(weight_));

        if (!is.read((char*)bias_, bias_bytes))
            return false;
        if (!is.read((char*)&weight_, weight_bytes))
            return false;

        return true;
    }

    void forward(const int8_t *input, int32_t *output) const {
        *output = bias_[0];
        for (size_t i = 0; i < INPUT_DIM; ++i)
            *output += static_cast<int16_t>(input[i]) * weight_[i];
    }

private:
    alignas(SIMD_ALIGN) int8_t weight_[INPUT_DIM];
    alignas(SIMD_ALIGN) int32_t bias_[OUTPUT_DIM];
};

