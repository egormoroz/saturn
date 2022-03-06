#include "search.hpp"
#include "../tt.hpp"
#include "../movgen/generate.hpp"
#include "eval.hpp"

int SearchContext::search(const Board &b, 
        int alpha, int beta, int depth, int ply) 
{
    if (stop())
        return 0;

    if (b.checkers())
        ++depth;

    if (depth == 0)
        return quiesce(alpha, beta, b);
    ++nodes_;

    if (hist_.is_repetition(b.key(), b.fifty_rule()))
        return 0;

    if (ply >= MAX_PLIES)
        return eval(b);

    TTEntry tte;
    if (g_tt.probe(b.key(), tte) == HASH_HIT) {
        if (tte.depth8 >= depth) {
            tt_hits_++;
            if (tte.bound8 == BOUND_EXACT)
                return tte.score16;
            if (tte.bound8 == BOUND_BETA && tte.score16 >= beta)
                return beta;
            if (tte.bound8 == BOUND_ALPHA && tte.score16 <= alpha)
                return alpha;
        }
    }

    ExtMove begin[MAX_MOVES], *end;
    end = generate<LEGAL>(b, begin);
    if (begin == end) {
        if (b.checkers())
            return mated_in(ply);
        return 0;
    }

    Bound bound = BOUND_ALPHA;
    Move best_move = MOVE_NONE;
    Board bb;
    for (auto *it = begin; it != end; ++it) {
        Move m = *it;
        bb = do_move(b, m);
        int score = -search(bb, -beta, -alpha, depth - 1, ply + 1);
        undo_move();        

        if (score > alpha) {
            if (score >= beta) {
                if (!stop_) {
                    g_tt.store(TTEntry(b.key(), beta, BOUND_BETA, 
                                depth, best_move, false));
                }
                return beta;
            }

            alpha = score;
            best_move = m;
            bound = BOUND_EXACT;
        }
    }

    if (!stop_) {
        g_tt.store(TTEntry(b.key(), alpha, bound, depth, 
                    best_move, false));
    }

    return alpha;
}

