#pragma once

#include <cstring>
#include <istream>
#include <type_traits>
#include "simd.hpp"
#include "nnue_misc.hpp"

constexpr size_t InputSimdWidth = 32;
constexpr size_t MaxNumOutputRegs = 8;

template<size_t InputDims, size_t OutputDims, typename Enabled = void>
struct LinearLayer;


template<size_t InputDims, size_t OutputDims>
struct LinearLayer<InputDims, OutputDims, 
    std::enable_if_t<(InputDims >= 128)>> 
{
    static_assert(InputDims % 32 == 0);
    static_assert(OutputDims % 32 == 0);
    static_assert(InputDims >= 128);

    static constexpr size_t NumOutputRegs = MaxNumOutputRegs;
    static constexpr size_t SmallBlockSize = InputSimdWidth;
    static constexpr size_t BigBlockSize = NumOutputRegs * InputDims;
    static constexpr size_t NumSmallBlocksInBigBlock = BigBlockSize / SmallBlockSize;
    static constexpr size_t NumSmallBlocksPerOutput = InputDims / SmallBlockSize;
    static constexpr size_t NumBigBlocks = OutputDims / NumOutputRegs;

    static constexpr size_t IN_DIM_ALIGNED = InputDims;
    static constexpr size_t OUT_DIM_ALIGNED = OutputDims;

    using InputBuffer = int8_t[IN_DIM_ALIGNED];
    using OutputBuffer = int32_t[OUT_DIM_ALIGNED];

    static size_t get_weight_index(size_t i) {
        const size_t small_block = (i / SmallBlockSize) % NumSmallBlocksInBigBlock;
        const size_t small_block_col = small_block / NumSmallBlocksPerOutput;
        const size_t small_block_row = small_block % NumSmallBlocksPerOutput;
        const size_t big_block = i / BigBlockSize;
        const size_t rest = i % SmallBlockSize;

        const size_t idx = 
              big_block * BigBlockSize
            + small_block_row * SmallBlockSize * NumOutputRegs
            + small_block_col * SmallBlockSize
            + rest;
        return idx;
    }

    bool load_parameters(std::istream &is) {
        is.read((char*)bias, 4 * OutputDims);

        for (size_t i = 0; i < InputDims * OutputDims; ++i)
            weight[get_weight_index(i)] = is.get();

        return !is.fail();
    }

    void propagate(const int8_t *input, int32_t *output) const {
        using acc_vec_t = __m256i;
        using bias_vec_t = __m128i;
        using weight_vec_t = __m256i;
        using in_vec_t = __m256i;

        const in_vec_t *invec = reinterpret_cast<const in_vec_t*>(input);

        for (size_t big_block = 0; big_block < NumBigBlocks; ++big_block) {
            acc_vec_t acc[NumOutputRegs] = { {0} };

            for (size_t small_block = 0; small_block < NumSmallBlocksPerOutput; 
                    small_block += 2)
            {
                const weight_vec_t *weightvec = 
                    reinterpret_cast<const weight_vec_t*>(
                          weight
                        + big_block * BigBlockSize
                        + small_block * SmallBlockSize * NumOutputRegs
                    );

                const in_vec_t in0 = invec[small_block + 0];
                const in_vec_t in1 = invec[small_block + 1];

                for (size_t k = 0; k < NumOutputRegs; ++k)
                    m256_add_dpbusd_epi32x2(acc[k], in0, weightvec[k],
                        in1, weightvec[k + NumOutputRegs]);
            }

            static_assert(NumOutputRegs % 4 == 0);
            bias_vec_t *outputvec = reinterpret_cast<bias_vec_t*>(output);
            const bias_vec_t *biasvec = 
                reinterpret_cast< const bias_vec_t*>(bias);

            for (size_t k = 0; k < NumOutputRegs; k += 4) {
                const size_t idx = (big_block * NumOutputRegs + k) / 4;
                outputvec[idx] = m256_haddx4(acc[k+0], acc[k+1],
                        acc[k+2], acc[k+3], biasvec[idx]);
            }
        }
    }

    int8_t weight[InputDims * OutputDims];
    int32_t bias[OutputDims];
};


template<size_t IN_DIM, size_t OUT_DIM>
struct LinearLayer<IN_DIM, OUT_DIM, 
    std::enable_if_t<(
        round_up<size_t>(IN_DIM, 32) < 128
        && OUT_DIM >= 4
    )>>
{
    static constexpr size_t INPUT_DIM = IN_DIM;
    static constexpr size_t OUTPUT_DIM = OUT_DIM;

    static_assert(OUTPUT_DIM % 4 == 0, "out_dim must be aligned");

    static constexpr size_t IN_DIM_ALIGNED = 
        round_up<size_t>(INPUT_DIM, 32);
    static constexpr size_t OUT_DIM_ALIGNED =
        round_up<size_t>(OUTPUT_DIM, 32);

    static_assert(IN_DIM_ALIGNED < 128);

    using InputBuffer = int8_t[IN_DIM_ALIGNED];
    using OutputBuffer = int32_t[OUT_DIM_ALIGNED];

    bool load_parameters(std::istream &is) {
        constexpr size_t bias_bytes = OUTPUT_DIM * 4;

        memset(biases, 0, sizeof(biases));
        if (!is.read((char*)biases, bias_bytes))
            return false;

        memset(weights, 0, sizeof(weights));
        for (size_t i = 0; i < OUTPUT_DIM; ++i) {
            size_t offset = IN_DIM_ALIGNED * i;
            if (!is.read((char*)&weights[offset], INPUT_DIM))
                return false;
        }

        return true;
    }

    void propagate(const int8_t *input, int32_t *output) const {
        constexpr size_t register_width = 256 / 8;
        static_assert(IN_DIM_ALIGNED % register_width == 0, "must be aligned");
        static_assert(OUTPUT_DIM % 4 == 0, "unrolling by 4");
        constexpr size_t num_in_chunks = IN_DIM_ALIGNED / register_width;        
        constexpr size_t num_out_chunks = OUTPUT_DIM / 4;

        for (size_t i = 0; i < num_out_chunks; ++i) {
            const size_t offset0 = (i * 4 + 0) * IN_DIM_ALIGNED;
            const size_t offset1 = (i * 4 + 1) * IN_DIM_ALIGNED;
            const size_t offset2 = (i * 4 + 2) * IN_DIM_ALIGNED;
            const size_t offset3 = (i * 4 + 3) * IN_DIM_ALIGNED;

            __m256i sum0 = _mm256_setzero_si256();
            __m256i sum1 = _mm256_setzero_si256();
            __m256i sum2 = _mm256_setzero_si256();
            __m256i sum3 = _mm256_setzero_si256();

            for (size_t j = 0; j < num_in_chunks; ++j) {
                const __m256i in = m256_load(&input[j * register_width]);

                m256_add_dpbusd_epi32(sum0, in, m256_load(&weights[offset0 + j * register_width]));
                m256_add_dpbusd_epi32(sum1, in, m256_load(&weights[offset1 + j * register_width]));
                m256_add_dpbusd_epi32(sum2, in, m256_load(&weights[offset2 + j * register_width]));
                m256_add_dpbusd_epi32(sum3, in, m256_load(&weights[offset3 + j * register_width]));
            }

            const __m128i bias = _mm_load_si128((const __m128i*)&biases[i * 4]);
            _mm_store_si128(
                (__m128i*)&output[i * 4], 
                m256_haddx4(sum0, sum1, sum2, sum3, bias)
            );
        }
    }

    alignas(32) int8_t weights[IN_DIM_ALIGNED * OUTPUT_DIM];
    alignas(32) int32_t biases[OUTPUT_DIM];
};

template<size_t IN_DIM>
struct LinearLayer<IN_DIM, 1, void> {
    static constexpr size_t INPUT_DIM = IN_DIM;
    static constexpr size_t OUTPUT_DIM = 1;

    using InputBuffer = int8_t[INPUT_DIM];

    bool load_parameters(std::istream &is) {
        constexpr size_t bias_bytes = OUTPUT_DIM * 4;
        constexpr size_t weight_bytes = INPUT_DIM * OUTPUT_DIM;

        memset(biases, 0, sizeof(biases));
        memset(weights, 0, sizeof(weights));

        if (!is.read((char*)biases, bias_bytes))
            return false;
        if (!is.read((char*)&weights, weight_bytes))
            return false;

        return true;
    }

    void propagate(const int8_t *input, int32_t *output) const {
        *output = biases[0];
        for (size_t i = 0; i < INPUT_DIM; ++i)
            *output += static_cast<int16_t>(input[i]) * weights[i];
    }

    alignas(32) int8_t weights[INPUT_DIM];
    alignas(32) int32_t biases[OUTPUT_DIM];
};

