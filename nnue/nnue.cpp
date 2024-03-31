#include <cstring>
#include <fstream>

#include "nnue.hpp"
#include "network.hpp"
#include "../board/board.hpp"
#include "state.hpp"

#include "../parameters.hpp"
#include "../scout.hpp"

#include "ops.hpp"

namespace nnue {


bool NNUE_INITIALIZED = false;

Network net;
TransformerLayer transformer;

struct AutoInit {
    AutoInit() {
        if (!nnue::load_parameters(params::defaults::nnue_weights_path))
            sync_cout() << "[WARN] failed to initialize nnue\n";
    }
} _;

void refresh_accumulator(
        const Board &b, 
        Accumulator &acc,
        Color perspective) 
{
    Square ksq = b.king_square(perspective);

    uint16_t features[32];
    int n_features = 0;

    Bitboard mask = b.pieces() & ~b.pieces(KING);
    while (mask) {
        Square s = pop_lsb(mask);
        Piece p = b.piece_on(s);

        features[n_features++] = halfkp::halfkp_idx(perspective, ksq, s, p);
    }

    transformer.refresh_accumulator(acc, 
        halfkp::FeatureSpan(features, features + n_features), perspective);
}

bool update_accumulator(
        StateInfo *si, 
        Color side,
        Square ksq) 
{
    if (si->acc.computed[side])
        return true;

    if (!si->previous || si == si->previous)
        return false;

    // deltas aren't initialized, so an extra check is needed
    if (si->nb_deltas && si->deltas[0].piece == make_piece(side, KING))
        return false;

    if (!update_accumulator(si->previous, side, ksq))
        return false;

    memcpy(si->acc.v[side], si->previous->acc.v[side], nnspecs::HALFKP * 2);
    si->acc.psqt[side] = si->previous->acc.psqt[side];

    if (!si->nb_deltas) {
        si->acc.computed[side] = true;
        return true;
    }

    uint16_t added[3], removed[3];
    int n_added = 0, n_removed = 0;

    for (int i = 0; i < si->nb_deltas; ++i) {
        const Delta &d = si->deltas[i];
        if (type_of(d.piece) == KING)
            continue;

        if (d.to != SQ_NONE) {
            uint16_t idx = halfkp::halfkp_idx(side, ksq, d.to, d.piece);
            added[n_added++] = idx;
        }

        if (d.from != SQ_NONE) {
            uint16_t idx = halfkp::halfkp_idx(side, ksq, d.from, d.piece);
            removed[n_removed++] = idx;
        }
    }

    transformer.update_accumulator(si->acc,
        halfkp::FeatureSpan(added, added + n_added),
        halfkp::FeatureSpan(removed, removed + n_removed),
        side
    );

    return true;
}


int32_t evaluate(const Board &b) {
    if (!NNUE_INITIALIZED) {
        sync_cout() << "info string NNUE not initialized, aborting...\n";
        std::abort();
    }

    StateInfo *si = b.get_stateinfo();
    Color stm = b.side_to_move();

    if (!update_accumulator(si, WHITE, b.king_square(WHITE)))
        refresh_accumulator(b, si->acc, WHITE);
    if (!update_accumulator(si, BLACK, b.king_square(BLACK)))
        refresh_accumulator(b, si->acc, BLACK);

    alignas(64) static thread_local int8_t transformed[nnspecs::L1_IN]{};

    scale_and_clamp<nnspecs::HALFKP>(si->acc.v[stm], transformed);
    scale_and_clamp<nnspecs::HALFKP>(si->acc.v[~stm], transformed + nnspecs::HALFKP);

    int32_t result = net.forward(transformed);
    result += (si->acc.psqt[stm] - si->acc.psqt[~stm]) / 2;

    return result;
}

bool load_parameters(const char *path) {
    NNUE_INITIALIZED = false;

    std::ifstream fin(path, std::ios::binary);
    if (!fin.is_open()) {
        sync_cout() << "Failed to open parameters file\n";
        return false;
    }

    if (!transformer.load_parameters(fin)) {
        sync_cout() << "Failed to load tranformer parameters\n";
        return false;
    }

    if (!net.load_parameters(fin)) {
        sync_cout() << "Failed to load net parameters\n";
        return false;
    }

    NNUE_INITIALIZED = true;
    return true;
}

} //nnue

