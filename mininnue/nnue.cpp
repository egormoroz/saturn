#include "nnue.hpp"
#include "ftset.hpp"
#include "layers.hpp"
#include "../primitives/utility.hpp"

#include "../board/board.hpp"
#include "../scout.hpp"

#include <fstream>
#include <streambuf>
#include <vector>

#include "../incbin.h"
#include "../pack.hpp"
#include "../parameters.hpp"


namespace mini {

INCBIN(uint8_t, _net, EVALFILE);


constexpr int S_A = 255;
constexpr int S_W = 64;
constexpr int S_O = 400;

bool NNUE_LOADED = false;

layers::Transformer<N_FEATURES, N_HIDDEN> ft;
layers::Output<N_HIDDEN, S_A> fc_out[2];
int16_t fc_out_bias;


namespace detail {

struct membuf : std::streambuf {
    membuf(char* begin, char* end) {
        this->setg(begin, begin, end);
    }

    pos_type seekoff(off_type off, std::ios_base::seekdir dir, 
            std::ios_base::openmode which = std::ios_base::in) override 
    {
        (void)(which);
        if (dir == std::ios_base::cur)
            gbump(off);
        else if (dir == std::ios_base::end)
            setg(eback(), egptr() + off, egptr());
        else if (dir == std::ios_base::beg)
            setg(eback(), eback() + off, egptr());
        return gptr() - eback();
    }

    pos_type seekpos(pos_type sp, std::ios_base::openmode which) override {
        return seekoff(sp - pos_type(off_type(0)), std::ios_base::beg, which);
    }
};

template<int block_size>
void decompress(BitReader &br, int16_t *params, int n_params) {
    for (int i = 0; i < n_params; ++i) {
        int off = 0;
        uint16_t x = 0;
        do {
            x |= br.read<uint16_t>(block_size) << off;
            off += block_size;

        } while (br.read<uint8_t>(1));

        int16_t sign = (x & 1) == 0 ? 1 : -1;

        params[i] = int16_t(x >> 1) * sign;
    }
}


bool load_parameters(std::istream &is) {
    NNUE_LOADED = false;

    if (!ft.load_parameters(is))
        return false;

    if (!is.read((char*)&fc_out_bias, sizeof(fc_out_bias)))
        return false;

    if (!fc_out[0].load_parameters(is))
        return false;
    if (!fc_out[1].load_parameters(is))
        return false;

    NNUE_LOADED = true;

    return true;
}

struct AutoInit {
    AutoInit() {
        const uint32_t num_params = *(const uint32_t*)&g_netData;

        BitReader br { g_netData + 4, 0 };
        std::vector<int16_t> unpacked(num_params);
        decompress<4>(br, unpacked.data(), num_params);

        membuf buf((char*)unpacked.data(), (char*)unpacked.data() + num_params * 2);
        std::istream is(&buf);

        if (!load_parameters(is))
            sync_cout() << "info string Default NNUE not loaded\n";
    };
} _;


} // detail




bool update_accumulator(StateInfo *si, Color pov, Square ksq) {
    if (si->acc.computed[pov])
        return true;

    if (!si->previous || si == si->previous)
        return false;

    const Delta d = si->deltas[0];
    if (si->nb_deltas && type_of(d.piece) == KING 
            && !same_king_bucket(pov, d.from, d.to))
        return false;

    if (!update_accumulator(si->previous, pov, ksq))
        return false;

    si->acc.psqt[pov] = si->previous->acc.psqt[pov];
    si->acc.v[pov] = si->previous->acc.v[pov];

    if (!si->nb_deltas) {
        si->acc.computed[pov] = true;
        return true;
    }

    uint16_t added[3], removed[3];
    int n_added = 0, n_removed = 0;

    for (int i = 0; i < si->nb_deltas; ++i) {
        const Delta &d = si->deltas[i];
        if (d.to != SQ_NONE)
            added[n_added++] = index(pov, d.to, d.piece, ksq);
        if (d.from != SQ_NONE)
            removed[n_removed++] = index(pov, d.from, d.piece, ksq);
    }

    ft.update_acc(si->acc, FtSpan(added, added + n_added), 
            FtSpan(removed, removed + n_removed), pov);

    si->acc.computed[pov] = true;
    return true;
}

void refresh_accumulator(const Board &b, Accumulator &acc, Color pov) {
    uint16_t features[32];
    int n_features = get_active_features(b, pov, features);

    ft.refresh_acc(acc, FtSpan(features, features + n_features), pov);
    acc.computed[pov] = true;
}

int32_t evaluate(const Board &b) {
    if (!NNUE_LOADED) {
        sync_cout() << "info string Attemped to evaluate "
            "without loaded nnue, aborting...\n";
        std::abort();
    }

    StateInfo *si = b.get_stateinfo();

    if (!update_accumulator(si, WHITE, b.king_square(WHITE)))
        refresh_accumulator(b, si->acc, WHITE);

    if (!update_accumulator(si, BLACK, b.king_square(BLACK)))
        refresh_accumulator(b, si->acc, BLACK);

    Color stm = b.side_to_move();
    int32_t psqt = (si->acc.psqt[stm] - si->acc.psqt[~stm]) / 2;
    int64_t result = 0;
    result += fc_out[0].forward(si->acc.v[stm].data());
    result += fc_out[1].forward(si->acc.v[~stm].data());

    return (result / S_A + fc_out_bias) * S_O / (S_W * S_A) + psqt;
}

bool load_parameters(const char *path) {
    std::ifstream fin(path, std::ios::binary);
    if (!fin.is_open()) {
        sync_cout() << "info string Failed to open weights file\n";
        NNUE_LOADED = false;
        return false;
    }

    return detail::load_parameters(fin);
}


} // mini

