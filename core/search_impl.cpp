#include "search.hpp"
#include "../tt.hpp"
#include "../movgen/generate.hpp"
#include "eval.hpp"
#include "../movepicker.hpp"
#include "defer.hpp"

namespace {

constexpr int CUTOFF_CHECK_DEPTH = 4;

constexpr bool DO_LMR = true;
constexpr bool DO_IID = true;
constexpr bool DO_REV_FUT = true;

}


int SearchContext::search_root(const Board &b, int alpha, 
        int beta, int depth)
{
    if (b.checkers())
        ++depth;

    TTEntry tte;
    Move ttm = MOVE_NONE;
    if (g_tt.probe(b.key(), tte) == HASH_HIT) {
        ttm = Move(tte.move16);
        if (!b.is_valid_move(ttm))
            ttm = MOVE_NONE;
    }

    //internal iterative deepening
    if (DO_IID && ttm == MOVE_NONE && depth > 7) {
        search<IS_PV>(b, alpha, beta, depth / 2, 0);
        if (g_tt.probe(b.key(), tte) == HASH_HIT
                && !b.is_valid_move((ttm = Move(tte.move16))))
            ttm = MOVE_NONE;
    }

    MovePicker mp(b, ttm, 0, MOVE_NONE, killers_, 
            counters_, history_);

    //maybe not MAX_MOVES??
    Move deferred_move[MAX_MOVES];
    int deferred_moves = 0;

    Bound bound = BOUND_ALPHA;
    Move best_move = MOVE_NONE;
    Board bb;
    int moves_processed = 0, score = alpha;
    for (Move m = mp.next(); m != MOVE_NONE; 
            m = mp.next(), ++moves_processed)
    {
        if (deferred_moves && depth >= CUTOFF_CHECK_DEPTH
            && g_tt.probe(b.key(), tte) == HASH_HIT
            && tte.depth8 >= depth) 
        {
            if (tte.bound8 == BOUND_BETA && tte.score16 >= beta)
                return beta;
            //Do we really need to check these?
            if (tte.bound8 == BOUND_EXACT)
                return tte.score16;
            if (tte.bound8 == BOUND_ALPHA && tte.score16 <= alpha)
                return alpha;
        }

        if (!moves_processed) {
            bb = b.do_move(m);
            hist_.push(b.key(), m);
            score = -search<IS_PV>(bb, -beta, -alpha, 
                    depth - 1, 1);
            hist_.pop();
        } else {
            uint32_t move_hash = abdada::move_hash(b.key(), m);

            if (abdada::defer_move(move_hash, depth)) {
                deferred_move[deferred_moves++] = m;
                continue;
            }

            bb = b.do_move(m);
            hist_.push(b.key(), m);
            abdada::starting_search(move_hash, depth);
            score = -search<NO_PV>(bb, -alpha - 1, 
                    -alpha, depth - 1, 1);
            abdada::finished_search(move_hash, depth);

            if (score > alpha && score < beta)
                score = -search<IS_PV>(bb, -beta, -alpha, 
                        depth - 1, 1);
            hist_.pop();

        }

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

    for (int i = 0; i < deferred_moves; ++i) {
        Move m = deferred_move[i];
        bb = b.do_move(m);
        hist_.push(b.key(), m);

        score = -search<NO_PV>(bb, -alpha - 1, -alpha,
                depth - 1, 1);
        if (score > alpha && score < beta)
            score = -search<IS_PV>(bb, -beta, -alpha,
                    depth - 1, 1);

        hist_.pop();

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
        return eval(b, alpha, beta);

    alpha = std::max(alpha, mated_in(ply));
    beta = std::min(beta, mate_in(ply + 1));
    if (alpha >= beta)
        return alpha;

    int stat_eval = eval(b);

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

        if ((tte.bound8 == BOUND_ALPHA && tte.score16 >= stat_eval)
                || (tte.bound8 == BOUND_BETA && tte.score16 <= stat_eval))
            stat_eval = tte.score16;

        if (!b.is_valid_move(ttm))
            ttm = MOVE_NONE;
    }

    //internal iterative deepening
    if (DO_IID && ttm == MOVE_NONE && depth > 7) {
        search<IS_PV>(b, alpha, beta, depth / 2, ply);
        if (g_tt.probe(b.key(), tte) == HASH_HIT
                && !b.is_valid_move((ttm = Move(tte.move16))))
            ttm = MOVE_NONE;
    }

    //Eval pruning / Static null move 
    if (DO_REV_FUT && depth < 3 && N == NO_PV && !b.checkers()
            && abs(beta - 1) > -VALUE_INFINITE + 100)
    {
        int eval_margin = 120 * depth;
        if (stat_eval - eval_margin >= beta)
            return stat_eval - eval_margin;
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

    bool f_prune = false;
    constexpr int FUT_MARGIN[4] = {0, 200, 300, 400};
    if (depth <= 3 && N == NO_PV && !b.checkers()
            && abs(alpha) < VALUE_MATE - 100 
            && stat_eval + FUT_MARGIN[depth] <= alpha)
        f_prune = true;


    Move prev = hist_.last_move();
    MovePicker mp(b, ttm, ply, prev, killers_, 
            counters_, history_);

    //maybe not MAX_MOVES??
    Move deferred_move[MAX_MOVES];
    int deferred_moves = 0;

    Bound bound = BOUND_ALPHA;
    Move best_move = MOVE_NONE;
    Board bb;
    int moves_processed = 0,
        local_best = -VALUE_INFINITE,
        score = -VALUE_INFINITE,
        new_depth = depth - 1,
        reduction_depth = 0;

    auto failed_high = [&](Move m, bool is_quiet) {
        fhf += moves_processed == 0;
        ++fh;

        if (is_quiet) {
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
    };

    for (Move m = mp.next(); m != MOVE_NONE; m = mp.next(), 
            ++moves_processed) 
    {
        bb = b.do_move(m);
        bool is_quiet = b.is_quiet(m);
        reduction_depth = 0;
        new_depth = depth - 1;

        if (f_prune && !bb.checkers() && is_quiet)
            continue;

        if (deferred_moves && depth >= CUTOFF_CHECK_DEPTH
            && g_tt.probe(b.key(), tte) == HASH_HIT
            && tte.depth8 >= depth) 
        {
            if (tte.bound8 == BOUND_BETA && tte.score16 >= beta)
                return beta;
            //Do we really need to check these?
            if (tte.bound8 == BOUND_EXACT)
                return tte.score16;
            if (tte.bound8 == BOUND_ALPHA && tte.score16 <= alpha)
                return alpha;
        }

        //Late move reduction
        if (DO_LMR && N == NO_PV && new_depth > 3 && moves_processed > 3
            && !b.checkers() && !bb.checkers() && is_quiet
            && killers_[0][ply] != m && killers_[1][ply] != m)
        {
            reduction_depth = 1;
            if (moves_processed > 8)
                reduction_depth = 2;
            new_depth -= reduction_depth;
        }

research:
        if (moves_processed == 0) {
            hist_.push(b.key(), m);
            score = -search<N>(bb, -beta, -alpha, 
                    new_depth, ply + 1);
            hist_.pop();
        } else {
            uint32_t move_hash = abdada::move_hash(b.key(), m);

            if (abdada::defer_move(move_hash, depth)) {
                deferred_move[deferred_moves++] = m;
                continue;
            }

            hist_.push(b.key(), m);
            abdada::starting_search(move_hash, depth);
            score = -search<NO_PV>(bb, -alpha - 1, -alpha,
                    new_depth, ply + 1);
            abdada::finished_search(move_hash, depth);

            if (score > alpha && score < beta)
                score = -search<IS_PV>(bb, -beta, -alpha,
                        new_depth, ply + 1);
            hist_.pop();
        }

        if (reduction_depth && score > alpha) {
            new_depth += reduction_depth;
            reduction_depth = 0;
            goto research;
        }

        if (score > local_best) {
            local_best = score;
            best_move = m;
        }

        if (score > alpha) {
            if (score >= beta) {
                failed_high(m, is_quiet);
                return beta;
            }

            alpha = score;
            best_move = m;
            bound = BOUND_EXACT;
        }
    }

    for (int i = 0; i < deferred_moves; ++i) {
        Move m = deferred_move[i];
        bb = b.do_move(m);
        hist_.push(b.key(), m);

        score = -search<NO_PV>(bb, -alpha - 1, -alpha,
                depth - 1, ply + 1);
        if (score > alpha && score < beta)
            score = -search<IS_PV>(bb, -beta, -alpha,
                    depth - 1, ply + 1);

        hist_.pop();

        if (score > local_best) {
            local_best = score;
            best_move = m;
        }

        if (score > alpha) {
            if (score >= beta) {
                failed_high(m, b.is_quiet(m));
                return beta;
            }

            alpha = score;
            best_move = m;
            bound = BOUND_EXACT;
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

