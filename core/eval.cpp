#include "eval.hpp"
#include "../board/board.hpp"

bool is_endgame(const Board &b) {
    //check if both sides have at least one big piece
    return (b.pieces(WHITE) & ~b.pieces(PAWN, KING))
        && (b.pieces(BLACK) & ~b.pieces(PAWN, KING));
}

int count_material(const Board &b, Bitboard mask, Color side) {
    int mat_score = 0;
    for (PieceType p: { PAWN, KNIGHT, BISHOP, ROOK, QUEEN })
        mat_score += popcnt(mask & b.pieces(side, p)) * PIECE_VALUE[p];

    return mat_score;
}

int eval(const Board &b) {
    Color us = b.side_to_move(), them = ~us;
    return count_material(b, ~Bitboard(0), us) 
        - count_material(b, ~Bitboard(0), them);
}

