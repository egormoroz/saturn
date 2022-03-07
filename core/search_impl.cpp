#include "search.hpp"
#include "../tt.hpp"
#include "../movgen/generate.hpp"
#include "eval.hpp"
#include "../movepicker.hpp"


int SearchContext::search_root(const Board &b, int alpha, 
        int beta, int depth)
{
    if (b.checkers())
        ++depth;

    TTEntry tte;
    Move ttm = MOVE_NONE;
    if (g_tt.probe(b.key(), tte) == HASH_HIT)
        ttm = Move(tte.move16);

    MovePicker mp(b, ttm, 0, MOVE_NONE, killers_, 
            counters_, history_);

    Bound bound = BOUND_ALPHA;
    Move best_move = MOVE_NONE;
    Board bb;
    int moves_processed = 0;
    for (Move m = mp.next(); m != MOVE_NONE; 
            m = mp.next(), ++moves_processed)
    {
        bb = do_move(b, m);
        int score = alpha;
        if (!moves_processed || -search<NO_PV>(bb, -alpha - 1, 
                    -alpha, depth - 1, 1) > alpha) 
            score = -search<IS_PV>(bb, -beta, -alpha, depth - 1, 1);
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

    if (!moves_processed) {
        if (b.checkers())
            return -VALUE_MATE;
        return 0;
    }

    if (!stop_) {
        g_tt.store(TTEntry(b.key(), alpha, bound, depth, 
                    best_move, false));
    }

    return alpha;
}


template<NodeType N>
int SearchContext::search(const Board &b, int alpha, int beta, 
        int depth, int ply, bool do_null)
{
    if (stop())
        return 0;

    if (b.checkers())
        ++depth;

    if (depth == 0)
        return quiesce(alpha, beta, b);

    if (hist_.is_repetition(b.key(), b.fifty_rule()))
        return 0;

    ++nodes_;
    if (ply >= MAX_PLIES)
        return eval(b);

    alpha = std::max(alpha, mated_in(ply));
    beta = std::min(beta, mate_in(ply + 1));
    if (alpha >= beta)
        return alpha;

    TTEntry tte;
    Move ttm = MOVE_NONE;
    if (g_tt.probe(b.key(), tte) == HASH_HIT) {
        ttm = Move(tte.move16);
        if (tte.depth8 >= depth) {
            tt_hits_++;
            if (tte.bound8 == BOUND_EXACT)
                return tte.score16;
            if (tte.bound8 == BOUND_BETA && tte.score16 >= beta)
                return beta;
            if (tte.bound8 == BOUND_ALPHA && tte.score16 <= alpha)
                return alpha;

            do_null = do_null && !tte.avoid_null;
        }
    }


    bool has_big_pieces = b.pieces(b.side_to_move())
        & ~b.pieces(PAWN, KING);
    bool avoid_null = false;
    if (do_null && N == NO_PV && depth > 2 && !b.checkers()
            && has_big_pieces)
    {
        int R = depth > 6 ? 3 : 2;
        int score = -search<NO_PV>(b.do_null_move(),
                -beta, -beta + 1, depth - 1 - R, ply, false);
        if (score >= beta)
            return beta;

        avoid_null = true;
    }


    Move prev = hist_.last_move();
    MovePicker mp(b, ttm, ply, prev, killers_, 
            counters_, history_);

    Bound bound = BOUND_ALPHA;
    Move best_move = MOVE_NONE;
    Board bb;
    int moves_processed = 0,
        local_best = -VALUE_INFINITE;
    bool raised_alpha = false;
    for (Move m = mp.next(); m != MOVE_NONE; m = mp.next(), 
            ++moves_processed) 
    {
        bb = do_move(b, m);

        int score = alpha;
        if (!raised_alpha) {
            score = -search<N>(bb, -beta, -alpha, 
                    depth - 1, ply + 1);
        } else if (-search<NO_PV>(bb, -alpha - 1, -alpha, 
                    depth - 1, ply + 1) > alpha) 
        {
            score = -search<IS_PV>(bb, -beta, -alpha, 
                    depth - 1, ply + 1);
        }

        undo_move();        

        //massive boost, no idea why I haven't been doing
        //this before...
        if (score > local_best) {
            local_best = score;
            best_move = m;
        }

        if (score > alpha) {
            if (score >= beta) {
                fhf += moves_processed == 0;
                ++fh;

                if ((type_of(m) == NORMAL || type_of(m) == CASTLING)
                        && !b.is_capture(m)) 
                {
                    if (killers_[0][ply] != m) {
                        killers_[1][ply] = killers_[0][ply];
                        killers_[0][ply] = m;
                    }

                    if (prev != MOVE_NONE)
                        counters_[from_sq(prev)][to_sq(prev)] = m;
                    history_[b.side_to_move()][from_sq(m)][to_sq(m)]
                        += depth * depth;
                }

                if (!stop_) {
                    g_tt.store(TTEntry(b.key(), beta, BOUND_BETA, 
                                depth, best_move, avoid_null));
                }
                return beta;
            }

            alpha = score;
            best_move = m;
            bound = BOUND_EXACT;
            raised_alpha = true;
        }
    }

    if (!moves_processed) {
        if (b.checkers())
            return mated_in(ply);
        return 0;
    }

    if (!stop_) {
        g_tt.store(TTEntry(b.key(), alpha, bound, depth, 
                    best_move, avoid_null));
    }

    return alpha;

}

