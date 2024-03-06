#pragma once

#include <cstdint>
#include "simd.hpp"

template<size_t N, int shift = 0>
void scale_and_clamp(const int16_t *input, int8_t *output) {
#if defined(USE_AVX2)
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
#elif defined(USE_SSSE3)
    constexpr int simd_reg_width = 128;
    constexpr int out_reg_width = simd_reg_width / 8;
    static_assert(N % out_reg_width == 0);
    constexpr int n_blocks = N / out_reg_width;

    auto in_vec = (const __m128i*)input;
    auto out_vec = (__m128i*)output;

    const __m128i k0x80s = _mm_set1_epi8(-128);
    for (int i = 0; i < n_blocks; ++i) {
        __m128i in0 = in_vec[2 * i + 0];
        __m128i in1 = in_vec[2 * i + 1];

        if constexpr (shift) {
            in0 = _mm_srai_epi16(in0, shift);
            in1 = _mm_srai_epi16(in1, shift);
        }

        __m128i x = _mm_packs_epi16(in0, in1);
        x = _mm_subs_epi8(_mm_adds_epi8(x, k0x80s), k0x80s);

        out_vec[i] = x;
    }
#endif
}

template<size_t N, int shift>
void scale_and_clamp(const int32_t *input, int8_t *output) {
#if defined(USE_AVX2)
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
#elif defined(USE_SSSE3)
    constexpr int simd_reg_width = 128;
    constexpr int out_reg_width = simd_reg_width / 8;
    static_assert(N % out_reg_width == 0);
    constexpr int n_blocks = N / out_reg_width;

    auto in_vec = (const __m128i*)input;
    auto out_vec = (__m128i*)output;

    const __m128i k0x80s = _mm_set1_epi8(-128);

    for (int i = 0; i < n_blocks; ++i) {
        __m128i in0 = _mm_packs_epi32(in_vec[4 * i + 0], in_vec[4 * i + 1]);
        __m128i in1 = _mm_packs_epi32(in_vec[4 * i + 2], in_vec[4 * i + 3]);

        if constexpr (shift) {
            in0 = _mm_srai_epi16(in0, shift);
            in1 = _mm_srai_epi16(in1, shift);
        }

        __m128i x = _mm_packs_epi16(in0, in1);
        x = _mm_subs_epi8(_mm_adds_epi8(x, k0x80s), k0x80s);
        out_vec[i] = x;
    }
#endif
}

