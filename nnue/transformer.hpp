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
        constexpr size_t psqt_bytes = INPUT_DIM * 4;

        memset(biases, 0, sizeof(biases));
        if (!is.read((char*)biases, bias_bytes))
            return false;

        memset(weights, 0, sizeof(weights));
        for (size_t i = 0; i < INPUT_DIM; ++i) {
            const size_t off = i * OUT_DIM_ALIGEND;
            if (!is.read((char*)&weights[off], OUTPUT_DIM * 2))
                return false;
        }

        memset(psqt_weights, 0, sizeof(psqt_weights));
        if (!is.read((char*)psqt_weights, psqt_bytes))
            return false;

        return true;
    }

    void refresh_accumulator(
            Accumulator &acc,
            FtSpan features, 
            Color perspective)
    {
        /* for (size_t i = 0; i < OUT_DIM_ALIGEND; ++i) */
        /*     acc.v[perspective][i] = biases[i]; */

        /* acc.psqt[perspective] = 0; */
        /* for (uint16_t idx: features) { */
        /*     for (size_t i = 0; i < OUT_DIM_ALIGEND; ++i) */
        /*         acc.v[perspective][i] += weights[OUT_DIM_ALIGEND * idx + i]; */

        /*     acc.psqt[perspective] += psqt_weights[idx]; */
        /* } */

        constexpr size_t register_width = 256 / 16;
        constexpr size_t num_chunks = OUT_DIM_ALIGEND / register_width;
        static_assert(num_chunks <= 16, "not enough registers");

        acc.psqt[perspective] = 0;
        int16_t *v = acc.v[perspective];
        __m256i regs[num_chunks];

        for (size_t i = 0; i < num_chunks; ++i) {
            regs[i] = _mm256_load_si256((const __m256i*)&biases[i * register_width]);
        }

        for (uint16_t idx: features) {
            const int16_t *wcol = &weights[idx * OUT_DIM_ALIGEND];
            for (size_t i = 0; i < num_chunks; ++i) {
                regs[i] = _mm256_add_epi16(
                    regs[i], 
                    *(const __m256i*)&wcol[ i * register_width]
                );
            }

            acc.psqt[perspective] += psqt_weights[idx];
        }

        for (size_t i = 0; i < num_chunks; ++i) {
            _mm256_store_si256((__m256i*)&v[i * register_width], regs[i]);
        }

        acc.computed[perspective] = true;
    }

    void update_accumulator(
            Accumulator &acc,
            FtSpan added, FtSpan removed,
            Color perspective)
    {
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

            acc.psqt[perspective] += psqt_weights[idx];
        }

        for (uint16_t idx: removed) {
            const int16_t *wcol = &weights[idx * OUT_DIM_ALIGEND];
            for (size_t i = 0; i < num_chunks; ++i) {
                regs[i] = _mm256_sub_epi16(
                    regs[i], 
                    *(const __m256i*)&wcol[i * register_width]
                );
            }

            acc.psqt[perspective] -= psqt_weights[idx];
        }

        for (size_t i = 0; i < num_chunks; ++i) {
            _mm256_store_si256((__m256i*)&v[i * register_width], regs[i]);
        }

        acc.computed[perspective] = true;
    }

    int16_t weights[INPUT_DIM * OUT_DIM_ALIGEND];
    int16_t biases[OUT_DIM_ALIGEND];

    int32_t psqt_weights[INPUT_DIM];
};

