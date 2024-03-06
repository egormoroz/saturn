#pragma once

#include <istream>
#include <immintrin.h>
#include "accumulator.hpp"
#include "nnue_misc.hpp"

using FtSpan = Span<uint16_t>;

template<size_t HALFK_FEATURES, size_t OUT_DIM>
struct FeatureTransformer {

    static constexpr size_t INPUT_DIM = HALFK_FEATURES;
    static constexpr size_t OUTPUT_DIM = OUT_DIM;

    static constexpr size_t OUT_DIM_ALIGEND =
        round_up<size_t>(OUTPUT_DIM, 16);


    bool load_parameters(std::istream &is) {
        constexpr size_t bias_bytes = OUTPUT_DIM * 2;

        memset(biases, 0, sizeof(biases));
        if (!is.read((char*)biases, bias_bytes))
            return false;

        memset(weights, 0, sizeof(weights));
        for (size_t i = 0; i < INPUT_DIM; ++i) {
            const size_t off = i * OUT_DIM_ALIGEND;
            if (!is.read((char*)&weights[off], OUTPUT_DIM * 2))
                return false;
        }

        return true;
    }

    void refresh_accumulator(
            Accumulator &acc,
            FtSpan features, 
            Color perspective)
    {
#if defined(USE_AVX2)

        constexpr size_t register_width = 256 / 16;
        constexpr size_t num_chunks = OUT_DIM_ALIGEND / register_width;
        static_assert(num_chunks <= 16, "not enough registers");

        int16_t *v = acc.v[perspective];
        __m256i regs[num_chunks];

        auto bias_vec = (const __m256i*)biases;
        for (size_t i = 0; i < num_chunks; ++i) 
            regs[i] = bias_vec[i];

        for (uint16_t idx: features) {
            auto weight_slice_vec = (const __m256i*)&weights[idx * OUT_DIM_ALIGEND];
            for (size_t i = 0; i < num_chunks; ++i)
                regs[i] = _mm256_add_epi16(regs[i], weight_slice_vec[i]);
        }

        for (size_t i = 0; i < num_chunks; ++i)
            *(__m256i*)&v[i * register_width] = regs[i];

#elif defined(USE_SSSE3)
        constexpr size_t register_width = 128 / 16;
        constexpr size_t n_registers = 8;
        constexpr size_t one_pass_size = register_width * n_registers;
        static_assert(OUT_DIM_ALIGEND % one_pass_size == 0);
        constexpr size_t n_passes = OUT_DIM_ALIGEND / one_pass_size;

        int16_t *v = acc.v[perspective];
        __m128i regs[n_registers];

        for (size_t i = 0; i < n_passes; ++i) {
            size_t offset = i * one_pass_size;
            auto bias_vec_slice = (const __m128i*)&biases[offset];
            auto acc_vec_slice = (__m128i*)&v[offset];

            for (size_t j = 0; j < n_registers; ++j) 
                regs[j] = bias_vec_slice[j];

            for (uint16_t idx: features) {
                auto weight_slice_vec = (const __m128i*)&weights[idx * OUT_DIM_ALIGEND + offset];
                for (size_t j = 0; j < n_registers; ++j)
                    regs[j] = _mm_add_epi16(regs[j], weight_slice_vec[j]);
            }

            for (size_t i = 0; i < n_registers; ++i)
                acc_vec_slice[i] = regs[i];
        }

#endif
        acc.computed[perspective] = true;
    }

    void update_accumulator(
            Accumulator &acc,
            FtSpan added, FtSpan removed,
            Color perspective)
    {
#if defined(USE_AVX2)
        constexpr size_t register_width = 256 / 16;
        constexpr size_t num_chunks = OUT_DIM_ALIGEND / register_width;
        static_assert(num_chunks <= 16, "not enough registers");

        int16_t *v = acc.v[perspective];
        
        __m256i regs[num_chunks];

        for (size_t i = 0; i < num_chunks; ++i) {
            regs[i] = _mm256_load_si256((const __m256i*)&v[i * register_width]);
        }

        for (uint16_t idx: added) {
            const int16_t *wcol = &weights[idx * OUT_DIM_ALIGEND];
            for (size_t i = 0; i < num_chunks; ++i) {
                regs[i] = _mm256_add_epi16(
                    regs[i], 
                    *(const __m256i*)&wcol[i * register_width]
                );
            }
        }

        for (uint16_t idx: removed) {
            const int16_t *wcol = &weights[idx * OUT_DIM_ALIGEND];
            for (size_t i = 0; i < num_chunks; ++i) {
                regs[i] = _mm256_sub_epi16(
                    regs[i], 
                    *(const __m256i*)&wcol[i * register_width]
                );
            }
        }

        for (size_t i = 0; i < num_chunks; ++i)
            _mm256_store_si256((__m256i*)&v[i * register_width], regs[i]);

#elif defined(USE_SSSE3)
        constexpr size_t register_width = 128 / 16;
        constexpr size_t n_registers = 8;
        constexpr size_t one_pass_size = register_width * n_registers;
        static_assert(OUT_DIM_ALIGEND % one_pass_size == 0);
        constexpr size_t n_passes = OUT_DIM_ALIGEND / one_pass_size;

        int16_t *v = acc.v[perspective];
        __m128i regs[n_registers];

        for (size_t i = 0; i < n_passes; ++i) {
            size_t offset = i * one_pass_size;
            auto acc_vec_slice = (__m128i*)&v[offset];

            for (size_t j = 0; j < n_registers; ++j) 
                regs[j] = acc_vec_slice[j];

            for (uint16_t idx: added) {
                auto weight_slice_vec = (const __m128i*)&weights[idx * OUT_DIM_ALIGEND + offset];
                for (size_t j = 0; j < n_registers; ++j)
                    regs[j] = _mm_add_epi16(regs[j], weight_slice_vec[j]);
            }

            for (uint16_t idx: removed) {
                auto weight_slice_vec = (const __m128i*)&weights[idx * OUT_DIM_ALIGEND + offset];
                for (size_t j = 0; j < n_registers; ++j)
                    regs[j] = _mm_sub_epi16(regs[j], weight_slice_vec[j]);
            }

            for (size_t i = 0; i < n_registers; ++i)
                acc_vec_slice[i] = regs[i];
        }
#endif

        acc.computed[perspective] = true;
    }

    alignas(SIMD_ALIGN) int16_t weights[INPUT_DIM * OUT_DIM_ALIGEND];
    alignas(SIMD_ALIGN) int16_t biases[OUT_DIM_ALIGEND];
};

