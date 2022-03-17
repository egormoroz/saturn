#include "search.hpp"
#include "../tt.hpp"
#include "eval.hpp"
#include "../movepicker.hpp"
#include "defer.hpp"
#include <algorithm>
#include "../tree.hpp"

namespace {

constexpr int CUTOFF_CHECK_DEPTH = 4;
//constexpr int NULL_MOVE_DIVISOR = 6;

constexpr bool DO_NULL = true;
constexpr bool DO_LMR = true;
constexpr bool DO_IIR = true;
constexpr bool DO_REV_FUT = true;
constexpr bool DO_RAZORING = true;

struct Tracer {
    Tracer(Move m, int alpha, int beta, int depth, int ply)
#ifdef TRACE
        : node_idx(g_tree.begin_node( m, alpha, beta, 
            depth, ply)), before(g_tree.size()) 
#endif
    {}

    void finish(int score) {
#ifdef TRACE
        g_tree.end_node(node_idx, score, before);
#endif
    }

    size_t node_idx{}, before{};
};

//returns true if it's safe to return score from search
template<bool full_probe>
bool probe_tt(const Board &b, int alpha, int beta, int depth, 
        int &score, bool *should_null = nullptr, 
        int *eval = nullptr, Move *ttm = nullptr)
{
    TTEntry tte;
    if (g_tt.probe(b.key(), tte) != HASH_HIT)
        return false;

    if (tte.depth8 >= depth) {
        if (tte.bound8 == BOUND_EXACT) {
            score = tte.score16;
            return true;
        }

        if (tte.bound8 == BOUND_BETA && tte.score16 >= beta) {
            score = beta;
            return true;
        }

        if (tte.bound8 == BOUND_ALPHA && tte.score16 <= alpha) {
            score = alpha;
            return true;
        }

        if constexpr (full_probe) {
            *should_null = *should_null && !tte.avoid_null;
            Bound b = Bound(tte.bound8);
            int s = tte.score16;
            if ((b == BOUND_ALPHA && s >= *eval)
                || (b == BOUND_BETA && s <= *eval))
                *eval = tte.score16;
        }
    }

    if constexpr (full_probe) {
        *ttm = Move(tte.move16);
        if (!b.is_valid_move(*ttm))
            *ttm = MOVE_NULL;
    }

    return false;
}

} //namespace


int SearchContext::search_root(const Board &b, int alpha, 
        int beta, int depth)
{
    if (b.checkers())
        ++depth;

#ifdef TRACE
    g_tree.clear();
#endif

    TTEntry tte;

    if (!root_moves_nb_) {
        Move ttm = MOVE_NONE;
        if (g_tt.probe(b.key(), tte) == HASH_HIT) 
            if (!b.is_valid_move(ttm))
                ttm = MOVE_NONE;

        MovePicker mp(b, ttm, 0, MOVE_NONE, killers_, 
                counters_, history_);
        for (Move m = mp.next(); m != MOVE_NONE; m = mp.next())
            root_moves_[root_moves_nb_++].m = m;
    } else {
        std::stable_sort(root_moves_, root_moves_ + root_moves_nb_);
        for (int i = 0; i < root_moves_nb_; ++i) {
            auto &m = root_moves_[i];
            m.prev_score = m.score;
            m.score = -VALUE_INFINITE;
        }
    }

    //maybe not MAX_MOVES??
    int deferred_move[MAX_MOVES];
    int deferred_moves = 0;

    Bound bound = BOUND_ALPHA;
    Move best_move = MOVE_NONE;
    Board bb;
    int moves_tried = 0, score = alpha;
    for (; moves_tried < root_moves_nb_; ++moves_tried) {
        RootMove &rm = root_moves_[moves_tried];
        Move m = root_moves_[moves_tried].m;

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

        if (!moves_tried) {
            bb = b.do_move(m);
            ss_.push(b.key(), m);
            Tracer tr(m, alpha, beta, depth - 1, 0);

            score = -search<IS_PV>(bb, -beta, -alpha, 
                    depth - 1, 1);
            tr.finish(score);

            ss_.pop();
        } else {
            uint32_t move_hash = abdada::move_hash(b.key(), m);

            if (abdada::defer_move(move_hash, depth)) {
                deferred_move[deferred_moves++] = moves_tried;
                continue;
            }

            bb = b.do_move(m);
            ss_.push(b.key(), m);


            Tracer tr(m, alpha, beta, depth - 1, 0);
            abdada::starting_search(move_hash, depth);
            score = -search<NO_PV>(bb, -alpha - 1, 
                    -alpha, depth - 1, 1);
            abdada::finished_search(move_hash, depth);

            if (score > alpha && score < beta)
                score = -search<IS_PV>(bb, -beta, -alpha, 
                        depth - 1, 1);
            
            tr.finish(score);
            ss_.pop();
        }

        rm.score = score;
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
        RootMove &rm = root_moves_[deferred_move[i]];
        Move m = rm.m;
        bb = b.do_move(m);
        ss_.push(b.key(), m);

        score = -search<NO_PV>(bb, -alpha - 1, -alpha,
                depth - 1, 1);
        if (score > alpha && score < beta)
            score = -search<IS_PV>(bb, -beta, -alpha,
                    depth - 1, 1);

        ss_.pop();

        rm.score = score;
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

    if (!moves_tried) {
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

    if (b.checkers() && (N == IS_PV || depth < 6))
        ++depth;

    if (depth == 0)
        return quiesce(alpha, beta, b);

    g_tt.prefetch(b.key());

    if (ss_.is_repetition(b.key(), b.fifty_rule()))
        return 0;

    ++nodes_;
    if (ply >= MAX_DEPTH)
        return eval(b, alpha, beta);

    alpha = std::max(alpha, mated_in(ply));
    beta = std::min(beta, mate_in(ply + 1));
    if (alpha >= beta)
        return alpha;

    int stat_eval = eval(b, alpha, beta);

    Move ttm = MOVE_NONE;
    int tmp;
    if (probe_tt<true>(b, alpha, beta, depth, tmp, 
                &do_null, &stat_eval, &ttm))
        return tmp;

    if (DO_IIR && ttm == MOVE_NONE) {
        if (N == IS_PV && depth >= 3)
            depth -= 2;
        if (N == NO_PV && depth >= 8)
            depth -= 1;
    }

    bool f_prune = false;
    bool avoid_null = false;
    if (N == NO_PV && !b.checkers()) {
        if (DO_REV_FUT && depth <= 6 && stat_eval - 50 * depth > beta
                && beta > -VALUE_MATE + 100)
            return stat_eval;

        if (DO_RAZORING && depth <= 3 
                && stat_eval + 200 * depth * depth <= alpha)
                /* && alpha < VALUE_MATE - 100) */ 
        {
            int score = quiesce(alpha - 1, alpha, b);
            if (score < alpha)
                return score;
        }

        bool has_big_pieces = b.pieces(b.side_to_move())
            & ~b.pieces(PAWN, KING);

        if (DO_NULL && do_null && depth >= 3 && has_big_pieces 
                && stat_eval >= beta) 
        {
            int R = 4 + depth / 6 + std::min((stat_eval - beta) / 256, 3);
            R = std::min(depth, R);

            Board bb = b.do_null_move();
            ss_.push(bb.key(), MOVE_NULL);
            Tracer tr(MOVE_NULL, alpha, beta, depth - R, ply);

            int score = -search<NO_PV>(bb,
                    -beta, -beta + 1, depth - R, ply + 1, false);

            tr.finish(score);
            ss_.pop();
            if (score >= beta)
                return beta;

            avoid_null = true;
        }

        constexpr int FUT_MARGIN[4] = {0, 200, 300, 400};
        if (depth <= 3 && N == NO_PV && !b.checkers()
                && abs(alpha) < VALUE_MATE - 100 
                && stat_eval + FUT_MARGIN[depth] <= alpha)
            f_prune = true;

    }

    Move prev = ss_.last_move();
    MovePicker mp(b, ttm, ply, prev, killers_, 
            counters_, history_);

    //maybe not MAX_MOVES??
    Move deferred_move[MAX_MOVES];
    int deferred_moves = 0;

    Bound bound = BOUND_ALPHA;
    Move best_move = MOVE_NONE;
    Board bb;
    int moves_tried = 0,
        best_score = -VALUE_INFINITE,
        score = -VALUE_INFINITE,
        new_depth = depth - 1,
        reduction_depth = 0;

    auto failed_high = [&](Move m, bool is_quiet) {
        fhf += moves_tried == 0;
        ++fh;

        if (is_quiet) {
            if (killers_[0][ply] != m) {
                killers_[1][ply] = killers_[0][ply];
                killers_[0][ply] = m;
            }

            if (prev != MOVE_NONE)
                counters_[from_sq(prev)][to_sq(prev)] = m;
            /* history_[b.side_to_move()][from_sq(m)][to_sq(m)] */
            /*     += depth * depth; */
        }

        if (!stop_) {
            g_tt.store(TTEntry(b.key(), beta, BOUND_BETA, 
                        depth, best_move, avoid_null));
        }
    };

//    b.validate();

    for (Move m = mp.next(); m != MOVE_NONE; m = mp.next(), 
            ++moves_tried) 
    {
        bb = b.do_move(m);
        bool is_quiet = b.is_quiet(m);
        reduction_depth = 0;
        new_depth = depth - 1;

        if (f_prune && !bb.checkers() && is_quiet)
            continue;

        //TODO: test me!
        if (deferred_moves && depth >= CUTOFF_CHECK_DEPTH && 
                probe_tt<false>(b, alpha, beta, depth, score))
            return score;

        score = search_move<N, true, true>(b, bb, m, alpha,
            beta, depth - 1, ply + 1, moves_tried,
            deferred_move, deferred_moves);

        if (score > best_score) {
            best_score = score;
            best_move = m;
        }

        if (score > alpha) {
            if (is_quiet) {
                history_[b.side_to_move()][from_sq(m)][to_sq(m)]
                    += depth * depth;
            }

            if (score >= beta) {
                failed_high(m, is_quiet);
                return beta;
            }

            alpha = score;
            bound = BOUND_EXACT;
        }
    }

    for (int i = 0; i < deferred_moves; ++i) {
        Move m = deferred_move[i];
        bb = b.do_move(m);
        ss_.push(b.key(), m);

        score = -search<NO_PV>(bb, -alpha - 1, -alpha,
                depth - 1, ply + 1);
        if (score > alpha && score < beta)
            score = -search<IS_PV>(bb, -beta, -alpha,
                    depth - 1, ply + 1);

        ss_.pop();

        if (score > best_score) {
            best_score = score;
            best_move = m;
        }

        if (score > alpha) {
            bool is_quiet = b.is_quiet(m);
            if (is_quiet) {
                history_[b.side_to_move()][from_sq(m)][to_sq(m)]
                    += depth * depth;
            }

            if (score >= beta) {
                failed_high(m, is_quiet);
                return beta;
            }

            alpha = score;
            bound = BOUND_EXACT;
        }
    }

    if (!moves_tried) {
        if (b.checkers())
            return mated_in(ply);
        return 0;
    }

    if (!stop_ && best_move != MOVE_NONE) {
        g_tt.store(TTEntry(b.key(), best_score, bound, depth, 
                    best_move, avoid_null));
    }

    return alpha;
}

template<NodeType N, bool reduce, bool defer>
int SearchContext::search_move(const Board &before, 
        const Board &after, Move played, 
        int alpha, int beta, int depth, int ply, int moves_tried,
        Move *deferred, int &deferred_moves)
{
    int score = 0, reduction = 0;

    if (!moves_tried) {
        ss_.push(before.key(), played);
        Tracer tr(played, alpha, beta, depth, ply);

        score = -search<N>(after, -beta, -alpha, depth, ply);
        
        tr.finish(score);
        ss_.pop();
    } else {
        uint32_t move_hash;
        if constexpr (defer) {
            move_hash = abdada::move_hash(before.key(), played);
            if (abdada::defer_move(move_hash, depth)) {
                deferred[deferred_moves++] = played;
                //failing low does nothing
                return alpha;
            }
        }

        if (reduce && DO_LMR && N == NO_PV 
            && depth > 3 && moves_tried > 3
            && !before.checkers() && !after.checkers() 
            && killers_[0][ply] != played 
            && killers_[1][ply] != played
            && before.is_quiet(played))
        {
            reduction = LMR[std::min(depth, 32)]
                [std::min(moves_tried, 63)];
            depth -= reduction;
        }

        ss_.push(before.key(), played);
        Tracer tr(played, alpha, beta, depth, ply);

        if constexpr (defer)
            abdada::starting_search(move_hash, depth);
        score = -search<NO_PV>(after, -alpha - 1, -alpha,
                depth, ply);
        if constexpr (defer)
            abdada::finished_search(move_hash, depth);

        if (score > alpha && score < beta)
            score = -search<IS_PV>(after, -beta, -alpha,
                    depth, ply);

        tr.finish(score);
        ss_.pop();
    }

    if (reduction && score > alpha)
        return search_move<N, false, defer>(before, after,
                played, alpha, beta, depth + reduction, ply, 
                moves_tried, deferred, deferred_moves);

    return score;
}

