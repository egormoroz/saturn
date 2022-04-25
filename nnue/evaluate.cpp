#include <cstring>
#include <fstream>

#include "evaluate.hpp"
#include "network.hpp"
#include "../board/board.hpp"
#include "nnue_state.hpp"

#include "crelu.hpp"

namespace {

Network net;
TransformerLayer transformer;

}

namespace nnue {

void refresh_accumulator(
        const Board &b, 
        Accumulator &acc,
        Color perspective) 
{
    Square ksq = b.king_square(perspective);
    ksq = relative_square(perspective, ksq);

    uint16_t features[32];
    int n_features = 0;

    Bitboard mask = b.pieces() & ~b.pieces(KING);
    while (mask) {
        Square s = pop_lsb(mask);
        Piece p = b.piece_on(s);

        features[n_features++] = halfkp_idx(ksq, 
                relative_square(perspective, s), p);
    }

    transformer.refresh_accumulator(acc, 
        FtSpan(features, features + n_features), perspective);
}

bool update_accumulator(
        StateInfo *si, 
        Color perspective,
        Square ksq) 
{
    if (si->acc.computed[perspective])
        return true;

    assert(si->previous);
    if (!update_accumulator(si->previous, perspective, ksq))
        return false;

    memcpy(si->acc.v[perspective], si->previous->acc.v[perspective], nnspecs::HALFKP * 2);
    si->acc.psqt[perspective] = si->previous->acc.psqt[perspective];

    if (!si->nb_deltas)
        return true;

    if (si->deltas[0].piece == make_piece(perspective, KING))
        return false;


    uint16_t added[3], removed[3];
    int n_added = 0, n_removed = 0;

    for (int i = 0; i < si->nb_deltas; ++i) {
        const Delta &d = si->deltas[i];
        if (type_of(d.piece) == KING)
            continue;

        if (d.to != SQ_NONE) {
            uint16_t idx = halfkp_idx(
                ksq, 
                relative_square(perspective, d.to),
                d.piece);
            added[n_added++] = idx;
        }

        if (d.from != SQ_NONE) {
            uint16_t idx = halfkp_idx(
                ksq, 
                relative_square(perspective, d.from),
                d.piece);
            removed[n_removed++] = idx;
        }
    }

    transformer.update_accumulator(si->acc,
        FtSpan(added, added + n_added),
        FtSpan(removed, removed + n_removed),
        perspective
    );

    return true;
}


int32_t evaluate(const Board &b) {
    StateInfo *si = b.get_stateinfo();
    Color stm = b.side_to_move();

    Square wksq = b.king_square(WHITE), 
           bksq = sq_mirror(b.king_square(BLACK));

    if (!update_accumulator(si, WHITE, wksq))
        refresh_accumulator(b, si->acc, WHITE);
    if (!update_accumulator(si, BLACK, bksq))
        refresh_accumulator(b, si->acc, BLACK);

    alignas(64) static thread_local int8_t transformed[nnspecs::L1_IN]{};

    scale_and_clamp<nnspecs::HALFKP>(si->acc.v[stm], transformed);
    scale_and_clamp<nnspecs::HALFKP>(si->acc.v[~stm], transformed + nnspecs::HALFKP);

    int32_t positional = net.propagate(transformed);
    int32_t psqt = (si->acc.psqt[stm] + si->acc.psqt[~stm]) / 2;
    psqt *= stm == WHITE ? 1 : -1;

    return positional + psqt;
}

bool load_parameters(const char *path) {
    std::ifstream fin(path, std::ios::binary);
    if (!fin.is_open()) {
        printf("failed to open parameters file\n");
        return false;
    }

    if (!transformer.load_parameters(fin)) {
        printf("failed to load tranformer parameters\n");
        return false;
    }

    if (!net.load_parameters(fin)) {
        printf("failed to load parameters\n");
        return false;
    }

    printf("successfully loaded nnue parameters\n");

    return true;
}

} //nnue

