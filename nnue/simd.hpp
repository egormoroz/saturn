#pragma once

#include <immintrin.h>
#include <cstdint>

inline __m256i m256_load(const void *data) {
    return _mm256_load_si256((const __m256i*)data);
}

inline void m256_add_dpbusd_epi32(
        __m256i &acc, 
        __m256i a, __m256i b)
{
    __m256i product = _mm256_maddubs_epi16(a, b);
    product = _mm256_madd_epi16(product, _mm256_set1_epi16(1));
    acc = _mm256_add_epi32(acc, product);
}

inline __m128i m256_haddx4(
    __m256i sum0,
    __m256i sum1,
    __m256i sum2,
    __m256i sum3,
    __m128i bias)
{
    sum0 = _mm256_hadd_epi32(sum0, sum1);
    sum2 = _mm256_hadd_epi32(sum2, sum3);

    sum0 = _mm256_hadd_epi32(sum0, sum2);

    __m128i sum128lo = _mm256_castsi256_si128(sum0);
    __m128i sum128hi = _mm256_extracti128_si256(sum0, 1);

    return _mm_add_epi32(_mm_add_epi32(sum128lo, sum128hi), bias);
}

inline void m256_add_dpbusd_epi32x2(
        __m256i &acc,
        __m256i a0, __m256i b0,
        __m256i a1, __m256i b1)
{
    __m256i product0 = _mm256_maddubs_epi16(a0, b0);
    __m256i product1 = _mm256_maddubs_epi16(a1, b1);
    product0 = _mm256_adds_epi16(product0, product1);
    product0 = _mm256_madd_epi16(product0, _mm256_set1_epi16(1));
    acc = _mm256_add_epi32(acc, product0);
}

inline int32_t m256_hadd(__m256i sum, int32_t bias) {
    __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(sum), _mm256_extracti128_si256(sum, 1));
    sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_PERM_BADC));
    sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_PERM_CDAB));
    return _mm_cvtsi128_si32(sum128) + bias;
}

