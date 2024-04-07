#ifndef LAYERS_HPP
#define LAYERS_HPP

#include "simd.hpp"
#include "state.hpp"
#include <istream>

namespace mini {

using FtSpan = Span<uint16_t>;

namespace layers {

template<int N_FEATURES, int N_HIDDEN>
struct Transformer {
    bool load_parameters(std::istream &is) {
        if (!is.read((char*)psqt_, sizeof(psqt_)))
            return false;

        if (!is.read((char*)bias_, sizeof(bias_)))
            return false;

        if (!is.read((char*)weight_, sizeof(weight_)))
            return false;

        return true;
    }

    void refresh_acc(Accumulator &acc, FtSpan features, Color pov) const {
        apply_features<true>(acc, features, FtSpan(), pov);
    }

    void update_acc(Accumulator &acc, FtSpan add, FtSpan sub, Color pov) const {
        apply_features<false>(acc, add, sub, pov);
    }

private:
    template<bool refresh>
    void apply_features(Accumulator &acc, FtSpan ft_add, FtSpan ft_sub, Color pov) const {
        constexpr int reg_width = simd_reg_width / 16;
        constexpr int n_regs = std::min(N_HIDDEN / reg_width, SIMD_REGISTERS);

        static_assert(N_HIDDEN % reg_width == 0);

        constexpr int pass_size = n_regs * reg_width;
        constexpr int n_passes = N_HIDDEN / pass_size;

        for (int pass = 0; pass < n_passes; ++pass) {
            const int off = pass * pass_size;
            auto acc_vec_slice = (SIMDVector*)&acc.v[pov][off];
            auto bias_vec_slice = (const SIMDVector*)&bias_[off];

            SIMDVector regs[n_regs];

            if constexpr (refresh) {
                for (int i = 0; i < n_regs; ++i)
                    regs[i] = bias_vec_slice[i];
            } else {
                for (int i = 0; i < n_regs; ++i)
                    regs[i] = acc_vec_slice[i];
            }

            for (uint16_t idx: ft_add) {
                auto column_vec = (const SIMDVector*)&weight_[N_HIDDEN * idx + off];
                for (int i = 0; i < n_regs; ++i)
                    regs[i] = vec_add_epi16(regs[i], column_vec[i]);
            }

            for (uint16_t idx: ft_sub) {
                auto column_vec = (const SIMDVector*)&weight_[N_HIDDEN * idx + off];
                for (int i = 0; i < n_regs; ++i)
                    regs[i] = vec_sub_epi16(regs[i], column_vec[i]);
            }

            for (int i = 0; i < n_regs; ++i)
                acc_vec_slice[i] = regs[i];
        }

        if (refresh)
            acc.psqt[pov] = 0;

        for (uint16_t idx: ft_add) acc.psqt[pov] += psqt_[idx];
        for (uint16_t idx: ft_sub) acc.psqt[pov] -= psqt_[idx];
    }

    alignas(SIMD_ALIGN) int16_t weight_[N_FEATURES * N_HIDDEN];
    alignas(SIMD_ALIGN) int16_t bias_[N_HIDDEN];
    int16_t psqt_[N_FEATURES];
};

template<int N_INPUT, int16_t S_A>
struct Output {
    bool load_parameters(std::istream &is) {
        if (!is.read((char*)weight_, sizeof(weight_)))
            return false;
        return true;
    }

    int32_t forward(const int16_t* x) const {
        const SIMDVector min{};
        const SIMDVector max = vec_set1_epi16(S_A);

        auto in_vec = (const SIMDVector*)x;
        auto weight_vec = (const SIMDVector*)weight_;

        // let's halve it just to have at least a few free registers
        constexpr int unroll_factor = SIMD_REGISTERS / 2;
        constexpr int chunk_size = simd_reg_width / 16;
        constexpr int n_chunks = N_INPUT / chunk_size;

        SIMDVector sums[unroll_factor]{};

        static_assert(n_chunks % unroll_factor == 0);

        for (int i = 0; i < n_chunks; i += unroll_factor) {

            for (int j = 0; j < unroll_factor; ++j) {
                auto u = vec_min_epi16(max, vec_max_epi16(min, in_vec[i + j]));
                sums[j] = vec_add_epi32(sums[j], vec_madd_epi16(u, weight_vec[i + j]));
            }

        }

        for (int i = unroll_factor / 2; i > 0; i /= 2)
            for (int j = 0; j < i; ++j)
                sums[j] = vec_hadd_epi32(sums[2 * j], sums[2 * j + 1]);

        return vec_hsum_epi32(sums[0]);
    }

private:
    alignas(SIMD_ALIGN) int16_t weight_[N_INPUT];
};


} // layers


} // mini


#endif
