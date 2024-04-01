#include "nnue.hpp"
#include "ftset.hpp"
#include "layers.hpp"

#include "../board/board.hpp"
#include "../scout.hpp"
#include "../parameters.hpp"

#include <cstring>
#include <fstream>


namespace mini {

constexpr int S_A = 256;
constexpr int S_W = 4096;

bool NNUE_LOADED = false;

layers::Transformer<N_FEATURES, N_HIDDEN> ft;
layers::Output<N_HIDDEN, S_A> fc_out[2];
int16_t fc_out_bias;

struct AutoInit {
    AutoInit() {
        if (!load_parameters(params::defaults::nnue_weights_path))
            sync_cout() << "info string Default NNUE not loaded\n";
    };
} _;

bool update_accumulator(StateInfo *si, Color pov) {
    if (si->acc.computed[pov])
        return true;

    if (!si->previous || si == si->previous)
        return false;

    if (!update_accumulator(si->previous, pov))
        return false;

    memcpy(si->acc.v[pov], si->previous->acc.v[pov], sizeof(Accumulator::v[0]));

    if (!si->nb_deltas) {
        si->acc.computed[pov] = true;
        return true;
    }

    uint16_t added[3], removed[3];
    int n_added = 0, n_removed = 0;

    for (int i = 0; i < si->nb_deltas; ++i) {
        const Delta &d = si->deltas[i];
        if (type_of(d.piece) == KING)
            continue;

        if (d.to != SQ_NONE)
            added[n_added++] = index(pov, d.to, d.piece);
        if (d.from != SQ_NONE)
            removed[n_removed++] = index(pov, d.from, d.piece);
    }

    ft.update_acc(si->acc, FtSpan(added, added + n_added), 
            FtSpan(removed, removed + n_removed), pov);

    si->acc.computed[pov] = true;
    return true;
}

void refresh_accumulator(const Board &b, Accumulator &acc, Color pov) {
    uint16_t features[32];
    int n_features = 0;

    Bitboard mask = b.pieces() & ~b.pieces(KING);
    while (mask) {
        Square s = pop_lsb(mask);
        features[n_features++] = index(pov, s, b.piece_on(s));
    }

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

    if (!update_accumulator(si, WHITE))
        refresh_accumulator(b, si->acc, WHITE);
    if (!update_accumulator(si, BLACK))
        refresh_accumulator(b, si->acc, BLACK);

    Color stm = b.side_to_move();
    int32_t result = fc_out_bias;
    result += fc_out[0].forward(si->acc.v[stm]);
    result += fc_out[1].forward(si->acc.v[~stm]);

    return result / S_W;
}

bool load_parameters(const char *path) {
    NNUE_LOADED = false;

    std::ifstream fin(path, std::ios::binary);
    if (!fin.is_open()) {
        sync_cout() << "info string Failed to open weights file\n";
        return false;
    }

    if (!ft.load_parameters(fin))
        return false;

    if (!fin.read((char*)&fc_out_bias, sizeof(fc_out_bias)))
        return false;

    if (!fc_out[0].load_parameters(fin))
        return false;
    if (!fc_out[1].load_parameters(fin))
        return false;

    NNUE_LOADED = true;
    return true;
}


} // mini

