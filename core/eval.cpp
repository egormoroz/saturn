#include "eval.hpp"
#include "../board/board.hpp"

namespace {

int eval_material(const Board &b, Color side) {
    int mat_score = 0;
    for (PieceType p: { PAWN, KNIGHT, BISHOP, ROOK, QUEEN })
        mat_score += popcnt(b.pieces(side, p)) * PIECE_VALUE[p];

    return mat_score;
}

} //namespace

int eval(const Board &b) {
    Color us = b.side_to_move(), them = ~us;
    return eval_material(b, us) - eval_material(b, them);
}

