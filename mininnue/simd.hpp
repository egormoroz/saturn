#pragma once

#include <immintrin.h>

#define RESTRICT __restrict

#if defined(__AVX2__)
#define USE_AVX2
#else 
#define USE_SSSE3
#endif

#if defined(USE_AVX2)

constexpr int SIMD_REGISTERS = 16;
constexpr int SIMD_ALIGN = 32;
constexpr int simd_reg_width = 256;

using SIMDVector = __m256i;

#define vec_add_epi32 _mm256_add_epi32

#define vec_add_epi16 _mm256_add_epi16
#define vec_sub_epi16 _mm256_sub_epi16
#define vec_madd_epi16 _mm256_madd_epi16
#define vec_hadd_epi32 _mm256_hadd_epi32
#define vec_mullo_epi16 _mm256_mullo_epi16

#define vec_set1_epi16 _mm256_set1_epi16

#define vec_min_epi16 _mm256_min_epi16
#define vec_max_epi16 _mm256_max_epi16

inline int32_t vec_hsum_epi32(__m256i x) {
    __m128i lo128 = _mm256_castsi256_si128(x);
    __m128i hi128 = _mm256_extracti128_si256(x, 1);

    __m128i sum128 = _mm_add_epi32(lo128, hi128);

    __m128i hi64 = _mm_unpackhi_epi64(sum128, sum128);
    __m128i sum64 = _mm_add_epi32(hi64, sum128);

    __m128i hi32 = _mm_shuffle_epi32(sum64, 0b00'00'00'01);

    return _mm_cvtsi128_si32(_mm_add_epi32(hi32, sum64));
}


#elif defined(USE_SSSE3)

constexpr int SIMD_REGISTERS = 8;
constexpr int SIMD_ALIGN = 16;
constexpr int simd_reg_width = 128;

using SIMDVector = __m128i;

#define vec_add_epi32 _mm_add_epi32

#define vec_add_epi16 _mm_add_epi16
#define vec_sub_epi16 _mm_sub_epi16

#define vec_madd_epi16 _mm_madd_epi16
#define vec_hadd_epi32 _mm_hadd_epi32

#define vec_mullo_epi16 _mm_mullo_epi16

#define vec_set1_epi16 _mm_set1_epi16

#define vec_min_epi16 _mm_min_epi16
#define vec_max_epi16 _mm_max_epi16

inline int32_t vec_hsum_epi32(__m128i sum128) {
    __m128i hi64 = _mm_unpackhi_epi64(sum128, sum128);
    __m128i sum64 = _mm_add_epi32(hi64, sum128);

    __m128i hi32 = _mm_shuffle_epi32(sum64, 0b00'00'00'01);

    return _mm_cvtsi128_si32(_mm_add_epi32(hi32, sum64));
}

#endif
