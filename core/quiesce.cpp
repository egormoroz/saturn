#include "search.hpp"
#include "../movgen/generate.hpp"
#include "eval.hpp"

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

    ExtMove begin[MAX_MOVES], 
            *end = generate<CAPTURES>(b, begin);
    Board bb;
    for (auto it = begin; it != end; ++it) {
        bb = b.do_move(*it);
        int score = -quiesce(-beta, -alpha, bb);
        if (score > alpha) {
            if (score >= beta)
                return beta;
            alpha = score;
        }
    }

    return alpha;
}

