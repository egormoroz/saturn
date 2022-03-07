#include "search.hpp"
#include "eval.hpp"
#include "../movepicker.hpp"

int SearchContext::quiesce(int alpha, int beta, 
        const Board &b) 
{
    ++nodes_;
    int stand_pat = eval(b);
    if (stand_pat >= beta) 
        return beta;
    if (stand_pat > alpha) 
        alpha = stand_pat;

    if (stop())
        return 0;

    MovePicker mp(b);
    Board bb;
    for (Move m = mp.qnext(); m != MOVE_NONE; m = mp.qnext()) {
        int cap_value = mg_value[type_of(b.piece_on(to_sq(m)))];
        cap_value += (type_of(m) == EN_PASSANT) * mg_value[PAWN];

        Color them = ~b.side_to_move();
        if (stand_pat + cap_value + 200 < alpha 
            && type_of(m) != PROMOTION 
            && count_material(b, them) - cap_value >= ENDGAME_MAT)
            continue;

        if (type_of(m) == NORMAL && !b.ok_capture(m))
            continue;

        bb = b.do_move(m);
        int score = -quiesce(-beta, -alpha, bb);
        if (score > alpha) {
            if (score >= beta)
                return beta;
            alpha = score;
        }
    }

    return alpha;
}

