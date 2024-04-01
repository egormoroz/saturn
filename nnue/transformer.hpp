#pragma once

#include <istream>
#include "simd.hpp"
#include "state.hpp"


namespace nnue {

template<size_t HALFK_FEATURES, size_t OUT_DIM>
struct FeatureTransformer {

    static constexpr size_t INPUT_DIM = HALFK_FEATURES;
    static constexpr size_t OUTPUT_DIM = OUT_DIM;


    bool load_parameters(std::istream &is) {
        constexpr size_t bias_bytes = OUTPUT_DIM * 2;
        constexpr size_t psqt_bytes = INPUT_DIM * 4;

        memset(bias_, 0, sizeof(bias_));
        if (!is.read((char*)bias_, bias_bytes))
            return false;

        memset(psqt_, 0, sizeof(psqt_));
        if (!is.read((char*)psqt_, psqt_bytes))
            return false;

        memset(weight_, 0, sizeof(weight_));
        for (size_t i = 0; i < INPUT_DIM; ++i) {
            const size_t off = i * OUT_DIM;
            if (!is.read((char*)&weight_[off], OUTPUT_DIM * 2))
                return false;
        }

        return true;
    }

    void refresh_accumulator(
            Accumulator &acc,
            halfkp::FeatureSpan features, 
            Color perspective)
    {
        acc.psqt[perspective] = 0;
        for (uint16_t idx: features)
            acc.psqt[perspective] += psqt_[idx];
#if defined(USE_AVX2)
        constexpr size_t register_width = 256 / 16;
        constexpr size_t num_chunks = OUT_DIM / register_width;
        static_assert(num_chunks <= 16, "not enough registers");

        int16_t *v = acc.v[perspective];
        __m256i regs[num_chunks];

        auto bias_vec = (const __m256i*)bias_;
        for (size_t i = 0; i < num_chunks; ++i) 
            regs[i] = bias_vec[i];

        for (uint16_t idx: features) {
            auto weight_slice_vec = (const __m256i*)&weight_[idx * OUT_DIM];
            for (size_t i = 0; i < num_chunks; ++i)
                regs[i] = _mm256_add_epi16(regs[i], weight_slice_vec[i]);
        }

        for (size_t i = 0; i < num_chunks; ++i)
            *(__m256i*)&v[i * register_width] = regs[i];

#elif defined(USE_SSSE3)
        constexpr size_t register_width = 128 / 16;
        constexpr size_t n_registers = 8;
        constexpr size_t one_pass_size = register_width * n_registers;
        static_assert(OUT_DIM % one_pass_size == 0);
        constexpr size_t n_passes = OUT_DIM / one_pass_size;

        int16_t *v = acc.v[perspective];
        __m128i regs[n_registers];

        for (size_t i = 0; i < n_passes; ++i) {
            size_t offset = i * one_pass_size;
            auto bias_vec_slice = (const __m128i*)&bias_[offset];
            auto acc_vec_slice = (__m128i*)&v[offset];

            for (size_t j = 0; j < n_registers; ++j) 
                regs[j] = bias_vec_slice[j];

            for (uint16_t idx: features) {
                auto weight_slice_vec = (const __m128i*)&weight_[idx * OUT_DIM + offset];
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
            halfkp::FeatureSpan added, halfkp::FeatureSpan removed,
            Color perspective)
    {
        for (uint16_t idx: added)
            acc.psqt[perspective] += psqt_[idx];
        for (uint16_t idx: removed)
            acc.psqt[perspective] -= psqt_[idx];

#if defined(USE_AVX2)
        constexpr size_t register_width = 256 / 16;
        constexpr size_t num_chunks = OUT_DIM / register_width;
        static_assert(num_chunks <= 16, "not enough registers");

        int16_t *v = acc.v[perspective];
        __m256i regs[num_chunks];

        for (size_t i = 0; i < num_chunks; ++i) {
            regs[i] = _mm256_load_si256((const __m256i*)&v[i * register_width]);
        }

        for (uint16_t idx: added) {
            const int16_t *wcol = &weight_[idx * OUT_DIM];
            for (size_t i = 0; i < num_chunks; ++i) {
                regs[i] = _mm256_add_epi16(
                    regs[i], 
                    *(const __m256i*)&wcol[i * register_width]
                );
            }
        }

        for (uint16_t idx: removed) {
            const int16_t *wcol = &weight_[idx * OUT_DIM];
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
        static_assert(OUT_DIM % one_pass_size == 0);
        constexpr size_t n_passes = OUT_DIM / one_pass_size;

        int16_t *v = acc.v[perspective];
        __m128i regs[n_registers];

        for (size_t i = 0; i < n_passes; ++i) {
            size_t offset = i * one_pass_size;
            auto acc_vec_slice = (__m128i*)&v[offset];

            for (size_t j = 0; j < n_registers; ++j) 
                regs[j] = acc_vec_slice[j];

            for (uint16_t idx: added) {
                auto weight_slice_vec = (const __m128i*)&weight_[idx * OUT_DIM + offset];
                for (size_t j = 0; j < n_registers; ++j)
                    regs[j] = _mm_add_epi16(regs[j], weight_slice_vec[j]);
            }

            for (uint16_t idx: removed) {
                auto weight_slice_vec = (const __m128i*)&weight_[idx * OUT_DIM + offset];
                for (size_t j = 0; j < n_registers; ++j)
                    regs[j] = _mm_sub_epi16(regs[j], weight_slice_vec[j]);
            }

            for (size_t i = 0; i < n_registers; ++i)
                acc_vec_slice[i] = regs[i];
        }
#endif

        acc.computed[perspective] = true;
    }

private:
    alignas(SIMD_ALIGN) int16_t weight_[INPUT_DIM * OUT_DIM];
    alignas(SIMD_ALIGN) int16_t bias_[OUT_DIM];
    int32_t psqt_[INPUT_DIM * 1];
};

}
