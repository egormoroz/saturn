#include <algorithm>
#include <cstring>
#include <cmath>

#include "search.hpp"
#include "eval.hpp"
#include "../movepicker.hpp"
#include "../primitives/utility.hpp"
#include "../tree.hpp"
#include "../tt.hpp"
#include "../nnue/evaluate.hpp"
#include "../nnue/nnue_state.hpp"
#include "../scout.hpp"

#define DO_NMP
#define DO_RFP
#define DO_LMR

/* #define EVAL_NOISE */

/* #define SOFT_LMR */

namespace {

uint8_t LMR[32][64];

bool can_return_ttscore(const TTEntry &tte, 
    int &alpha, int beta, int depth, int ply)
{
    if (tte.depth5 < depth)
        return false;

    int tt_score = tte.score(ply);
    if (tte.bound2 == BOUND_EXACT) {
        alpha = tt_score;
        return true;
    }
    if (tte.bound2 == BOUND_ALPHA && tt_score <= alpha)
        return true;
    if (tte.bound2 == BOUND_BETA && tt_score >= beta) {
        alpha = beta;
        return true;
    }

    return false;
}

Bound determine_bound(int alpha, int beta, int old_alpha) {
    if (alpha >= beta) return BOUND_BETA;
    if (alpha > old_alpha) return BOUND_EXACT;
    return BOUND_ALPHA;
}

int16_t cap_value(const Board &b, Move m) {
    if (type_of(m) == EN_PASSANT)
        return mg_value[PAWN];
    return mg_value[type_of(b.piece_on(to_sq(m)))];
}

} //namespace

void init_reduction_tables() {
    for (int depth = 1; depth < 32; ++depth)
        for (int moves = 1; moves < 64; ++moves)
            LMR[depth][moves] = static_cast<uint8_t>(
                0.1 + log(depth) * log(moves) / 2
            );
    LMR[0][0] = LMR[0][1] = LMR[1][0] = 0;

#ifdef SOFT_LMR
    memset(LMR, 0, sizeof(LMR));
#endif
}

void RootMovePicker::reset(const Board &root){
    Move ttm = MOVE_NONE;
    TTEntry tte;
    if (g_tt.probe(root.key(), tte)) {
        if (!root.is_valid_move(ttm = Move(tte.move16)))
            ttm = MOVE_NONE;
    }

    MovePicker mp(root, ttm);
    mpv_start_ = cur_ = num_moves_ = 0;
    for (Move m = mp.next<false>(); m != MOVE_NONE; 
            m = mp.next<false>())
    {
        moves_[num_moves_++] = { m, 0 };
    }
}

Move RootMovePicker::next() {
    if (cur_ >= num_moves_)
        return MOVE_NONE;
    return moves_[cur_++].move;
}

int RootMovePicker::num_moves() const {
    return num_moves_;
}

void RootMovePicker::complete_iter(int best_move_idx) {
    best_move_idx += mpv_start_;
    for (int i = best_move_idx; i > mpv_start_; --i)
        std::swap(moves_[i], moves_[i - 1]);

    cur_ = mpv_start_;
}

void RootMovePicker::complete_iter(Move m) {
    for (int i = mpv_start_; i < num_moves_; ++i) {
        if (moves_[i].move == m) {
            complete_iter(i - mpv_start_);
            return;
        }
    }
    assert(false);
}

void RootMovePicker::mpv_reset() {
    mpv_start_ = 0;
    cur_ = 0;
}

bool RootMovePicker::is_excluded(Move m) const {
    for (int i = 0; i < mpv_start_; ++i)
        if (moves_[i].move == m)
            return true;
    return false;
}

void RootMovePicker::exclude_top_move(int score) { 
    assert(mpv_start_ < num_moves_);
    moves_[mpv_start_++].score = score;
    cur_ = mpv_start_;
}

RootMove RootMovePicker::get_move(int idx) const { 
    assert(idx < num_moves_);
    return moves_[idx];
}

int RootMovePicker::num_excluded_moves() const { return mpv_start_; }

RootMove RootMovePicker::best_move() const {
    RootMove result = moves_[0];
    if (!num_moves_)
        result.move = MOVE_NONE;
    return result;
}

Search::Search() 
    : root_(Board::start_pos(&root_si_))
{
}

void Search::set_silent(bool s) {
    silent_ = s;
}

void Search::setup(const Board &root,  
        const SearchLimits &limits,
        UCISearchConfig usc, const Stack *st)
{
    // should we really do this?
    g_tt.new_search();

    uci_cfg_ = usc;
    root_ = root;
    limits_ = limits;
    stats_.reset();
    rmp_.reset(root_);
    hist_.reset();

    if (st)
        stack_ = *st;
    else
        stack_.reset();

    root_.set_stateinfo(&root_si_);
    root_si_.previous = nullptr;
    nnue::refresh_accumulator(root_, root_si_.acc, WHITE);
    nnue::refresh_accumulator(root_, root_si_.acc, BLACK);

    man_.init(limits, root.side_to_move(), stack_.total_height());

    memset(counters_.data(), 0, sizeof(counters_));
    memset(followups_.data(), 0, sizeof(followups_));

    keep_going_ = true;
}

bool Search::keep_going() {
    if (stats_.nodes % 2048 == 0 && keep_going_) {
        keep_going_ = limits_.infinite || !man_.out_of_time()
            || stats_.id_detph <= limits_.min_depth;
        if (limits_.max_nodes)
            keep_going_ = keep_going_ && stats_.nodes < limits_.max_nodes;
    }
    return keep_going_;
}

void Search::iterative_deepening() {
    int score = 0, prev_score;
    int n_pvs = std::min(uci_cfg_.multipv, rmp_.num_moves());

    if (!n_pvs){ 
        if (!silent_)
            sync_cout() << "no legal moves\n";
        n_pvs_ = 0;
    }

    if ((rmp_.num_moves() == 1 || is_draw()) && !silent_) {
        RootMove m = rmp_.best_move();
        sync_cout() << "bestmove " << m.move << '\n';
        return;
    }

    stats_.id_detph = 1;
    rmp_.mpv_reset();
    for (int i = 0; i < n_pvs; ++i) {
        score = search_root(-VALUE_MATE, VALUE_MATE, 1);
        rmp_.exclude_top_move(score);
    }
    extract_pvmoves();
    uci_report();


    for (int d = 2; d <= limits_.max_depth; ++d) {
        stats_.id_detph = d;
        g_tree.clear();
        prev_score = score;
        TimePoint start = timer::now();

        rmp_.mpv_reset();
        for (int i = 0; i < n_pvs; ++i) {
            score = aspiration_window(score, d);
            if (!keep_going())
                break;

            rmp_.exclude_top_move(score);
        }

        if (!keep_going())
            break;

        assert(rmp_.num_excluded_moves() == n_pvs);
        extract_pvmoves();
        uci_report();

        TimePoint now = timer::now(), 
              time_left = man_.start + man_.max_time - now;
        if (abs(score - prev_score) < 8 && !limits_.infinite
                && !limits_.move_time && now - start >= time_left)
            break; //assume we don't have enough time to go 1 ply deeper

        if (uci_cfg_.multipv == 1 && abs(score) >= VALUE_MATE - d)
            break;
    }

    RootMove rm = rmp_.best_move();
    if (!silent_)
        sync_cout() << "bestmove " << rm.move << '\n';
}

void Search::atomic_stop() {
    keep_going_ = false;
}

RootMove Search::get_pv_start(int i) const { 
    assert(i < n_pvs_);
    return pv_moves_[i]; 
}

int Search::num_pvs() const { return n_pvs_; }

int Search::aspiration_window(int score, int depth) {
    if (depth < uci_cfg_.asp_min_depth)
        return search_root(-VALUE_MATE, VALUE_MATE, depth);

    int delta = uci_cfg_.asp_init_delta, alpha = score - delta, 
        beta = score + delta;
    while (keep_going()) {
        if (alpha <= -3000) alpha = -VALUE_MATE;
        if (beta >= 3000) beta = VALUE_MATE;

        score = search_root(alpha, beta, depth);

        if (score <= alpha) {
            beta = (alpha + beta) / 2;
            alpha = std::max(-VALUE_MATE, alpha - delta);
        } else if (score >= beta) {
            beta = std::min(+VALUE_MATE, beta + delta);
        } else {
            break;
        }

        delta += delta / 2;
    }

    return score;
}

int Search::search_root(int alpha, int beta, int depth) {
    Move best_move = MOVE_NONE;
    int best_score = -VALUE_MATE, old_alpha = alpha,
        moves_tried = 0, best_move_idx = 0;
    StateInfo si;
    Board bb(&si);
    for (Move m = rmp_.next(); m != MOVE_NONE; m = rmp_.next()) {
        size_t ndx = g_tree.begin_node(m, alpha, beta, depth - 1, 0);
        bb = root_.do_move(m, &si);
        stack_.push(root_.key(), m);

        int score;
        if (!moves_tried) {
            score = -search(bb, -beta, -alpha, depth - 1);
        } else {

            int new_depth = depth - 1;
            int r = 0;

            // let's try mini LMR at the root
            // Allright, this tiny change made engine a lot stronger, huh...
            if (!bb.checkers() && root_.is_quiet(m) && moves_tried > 10
                    && depth > 6)
                ++r;

            new_depth = std::max(1, new_depth - r);

            score = -search(bb, -(alpha + 1), -alpha, new_depth);
            if (score > alpha && score < beta)
                score = -search(bb, -beta, -alpha, depth - 1);
        }

        ++moves_tried;
        stack_.pop();
        g_tree.end_node(ndx, score);

        if (score > best_score) {
            best_score = score;
            best_move = m;
            best_move_idx = moves_tried - 1;
        }

        if (score > alpha)
            alpha = score;
        if (score >= beta) {
            alpha = beta;
            break;
        }
    }
    

    if (keep_going()) {
        if (rmp_.num_excluded_moves() == 0)  {
            g_tt.store(TTEntry(root_.key(), alpha, evaluate(root_),
                determine_bound(alpha, beta, old_alpha),
                depth, best_move, 0, false));
        }

        rmp_.complete_iter(best_move_idx);
    }

    return alpha;
}

int Search::search(const Board &b, int alpha, 
        int beta, int depth) 
{
    const int ply = stack_.height();
    const bool pv = alpha != beta - 1;

    if (!keep_going())
        return 0;

    //Mate distance pruning
    int mated_score = stack_.mated_score();
    alpha = std::max(alpha, mated_score);
    beta = std::min(beta, -mated_score - 1);
    if (alpha >= beta)
        return alpha;

    if (depth <= 0)
        return b.checkers() ? quiescence<true>(b, alpha, beta)
            : quiescence<false>(b, alpha, beta);

    stats_.nodes++;
    stats_.sel_depth = std::max(stats_.sel_depth, ply);

    auto &entry = stack_.at(ply);
    g_tt.prefetch(b.key());
    if (b.half_moves() >= 100 
        || (!b.checkers() && b.is_material_draw())
        || stack_.is_repetition(b))
        return 0;

    TTEntry tte;
    bool avoid_null = false;
    Move ttm = MOVE_NONE;
    int16_t eval;
    if (g_tt.probe(b.key(), tte)) {
        if (ttm = Move(tte.move16); !b.is_valid_move(ttm))
            ttm = MOVE_NONE;
        
        if (can_return_ttscore(tte, alpha, beta, 
                    depth, ply))
        {
            if (ttm && b.is_quiet(ttm))
                hist_.add_bonus(b, ttm, depth * depth);
            return alpha;
        }

        avoid_null = tte.avoid_null;
        eval = tte.eval16;
    } else {
        eval = evaluate(b);
    }

    StateInfo si;
    /* int16_t eval = evaluate(b); */
    bool improving = !b.checkers() && ply >= 2 
        && stack_.at(ply - 2).eval < eval;

    if (depth >= 4 && !ttm)
        --depth;

    if (pv || b.checkers())
        goto move_loop; //skip pruning

    //Reverse futility pruning
#ifdef DO_RFP
    if (depth < 7 && eval - 175 * depth / (1 + improving) >= beta
            && abs(beta) < MATE_BOUND)
        return eval;
#endif

    //Null move pruning
#ifdef DO_NMP
    if (depth >= 3
        && b.plies_from_null() && !avoid_null
        && b.has_nonpawns(b.side_to_move())
        && eval >= beta)
    {
        int R = 3 + depth / 6, n_depth = depth - R - 1;
        size_t ndx = g_tree.begin_node(MOVE_NULL, alpha, 
                beta, n_depth, ply, NodeType::Null);
        stack_.push(b.key(), MOVE_NULL, eval);

        int score = -search(b.do_null_move(&si), -beta, 
                -beta + 1, n_depth);

        stack_.pop();
        g_tree.end_node(ndx, score);

        if (score >= beta)
            return beta;

        avoid_null = true;
    }
#endif

move_loop:
    Move opp_move = stack_.at(ply - 1).move,
         prev = MOVE_NONE, followup = MOVE_NONE,
         counter = counters_[from_to(opp_move)];
    if (ply >= 2) {
        prev = stack_.at(ply - 2).move;
        followup = followups_[from_to(prev)];
    }
    MovePicker mp(b, ttm, entry.killers, &hist_,
            counter, followup);

    Board bb(&si);
    auto search_move = [&](Move m, int depth, bool zw) {
        size_t ndx = g_tree.begin_node(m, alpha, beta, 
                depth, ply);
        int t_beta = zw ? -(alpha + 1) : -beta;
        int score = -search(bb, t_beta, -alpha, depth);
        g_tree.end_node(ndx, score);
        return score;
    };

    std::array<Move, 64> quiets;
    int num_quiets{};
    int best_score = -VALUE_MATE, moves_tried = 0,
        old_alpha = alpha, score = 0;
    Move best_move = MOVE_NONE;
    for (Move m = mp.next<false>(); m != MOVE_NONE; 
            m = mp.next<false>()) 
    {
        bool is_quiet = b.is_quiet(m);
        int new_depth = depth - 1, r = 0;
        bool killer_or_counter = m == counter
            || entry.killers[0] == m || entry.killers[1] == m;
        bb = b.do_move(m, &si);

        if (bb.checkers() && b.see_ge(m))
            new_depth++;

        int lmp_threshold = (3 + 2 * depth * depth) / (2 - improving);
        if (!pv && !bb.checkers() && is_quiet
                && moves_tried > lmp_threshold) 
            break;

#ifdef DO_LMR
        if (depth > 2 && moves_tried > 1 && is_quiet) {
            r = LMR[std::min(31, depth)][std::min(63, moves_tried)];
            if (!pv) ++r;
            if (!improving) ++r;
            if (killer_or_counter) r -= 2;
            if (bb.checkers()) --r;

            r -= hist_.get_score(b, m) / 8192;

            r = std::clamp(r, 0, new_depth - 1);
            new_depth -= r;
        }
#endif

        stack_.push(b.key(), m, eval);

        //Zero-window search
        if (!pv || moves_tried)
            score = search_move(m, new_depth, true);

        //Re-search if reduced move beats alpha
        if (r && score > alpha) {
            new_depth += r;
            score = search_move(m, new_depth, true);
        }

        //(Re-)search with full window
        if (pv && ((score > alpha && score < beta) || !moves_tried))
            score = search_move(m, new_depth, false);

        stack_.pop();
        ++moves_tried;

        if (score > best_score) {
            best_score = score;
            best_move = m;
        }

        if (b.is_quiet(m) && num_quiets < 64)
            quiets[num_quiets++] = m;

        if (score > alpha)
            alpha = score;
        if (score >= beta)
            break;
    }

    if (!moves_tried) {
        if (b.checkers())
            return stack_.mated_score();
        return 0;
    }

    if (alpha >= beta) {
        alpha = beta;
        stats_.fail_high++;
        stats_.fail_high_first += moves_tried == 1;
        hist_.update(b, best_move, depth, 
                quiets.data(), num_quiets);
        if (b.is_quiet(best_move)) {
            if (entry.killers[0] != best_move) {
                entry.killers[1] = entry.killers[0];
                entry.killers[0] = best_move;
            }

            counters_[from_to(opp_move)] = best_move;

            if (prev)
                followups_[from_to(prev)] = best_move;
        }
    }

    if (keep_going()) {
        g_tt.store(TTEntry(b.key(), alpha, eval,
            determine_bound(alpha, beta, old_alpha),
            depth, best_move, ply, avoid_null));
    }

    return alpha;
}

template<bool with_evasions>
int Search::quiescence(const Board &b, 
        int alpha, int beta) 
{
    keep_going();
    if (!keep_going() || b.half_moves() >= 100
        || b.is_material_draw()
        || stack_.is_repetition(b))
        return 0;

    stats_.nodes++;
    stats_.qnodes++;

    //Mate distance pruning
    int mated_score = stack_.mated_score();
    alpha = std::max(alpha, mated_score);
    beta = std::min(beta, -mated_score - 1);
    if (alpha >= beta)
        return alpha;

    int16_t eval = 0;
    if constexpr (!with_evasions) {
        eval = evaluate(b);
        alpha = std::max(alpha, +eval);
        if (alpha >= beta)
            return beta;
    }

    StateInfo si;
    MovePicker mp(b);
    Board bb(&si);
    constexpr bool only_tacticals = !with_evasions;
    int moves_tried = 0;
    for (Move m = mp.next<only_tacticals>(); m != MOVE_NONE; 
            m = mp.next<only_tacticals>(), ++moves_tried)
    {
        if (!with_evasions && type_of(m) != PROMOTION
                && eval + cap_value(b, m) + 200 <= alpha) 
            continue;

        size_t ndx = g_tree.begin_node(m, alpha, beta, 
                0, stack_.height());
        bb = b.do_move(m, &si);
        stack_.push(b.key(), m, eval);

        //filter out perpetual checks
        bool gen_evasions = !with_evasions && bb.checkers();
        int score = gen_evasions ? -quiescence<true>(bb, -beta, -alpha)
            : -quiescence<false>(bb, -beta, -alpha);

        stack_.pop();
        g_tree.end_node(ndx, score);

        if (score > alpha)
            alpha = score;
        if (score >= beta) 
            return beta;
    }

    if (with_evasions && !moves_tried)
        return stack_.mated_score();

    return alpha;
}

bool Search::is_draw() const {
    if (root_.half_moves() >= 100 
        || (!root_.checkers() && root_.is_material_draw())
        || stack_.is_repetition(root_))
        return true;
    return false;
}

int16_t Search::evaluate(const Board &b) {
#ifndef NONNUE
    int16_t result;
    if (!ev_cache_.probe(b.key(), result)) {
        result = static_cast<int16_t>(nnue::evaluate(b));
        ev_cache_.store(b.key(), result);
    }

    return result;
#else
    int16_t score = ::evaluate(b);
#ifdef EVAL_NOISE
    score += stats_.nodes % 8;
#endif
    return score;
#endif
}

void Search::extract_pvmoves() {
    for (n_pvs_ = 0; n_pvs_ < rmp_.num_excluded_moves(); ++n_pvs_)
        pv_moves_[n_pvs_] = rmp_.get_move(n_pvs_);
    std::stable_sort(pv_moves_, pv_moves_ + n_pvs_,
        [](const RootMove &x, const RootMove &y) { return x.score > y.score; });
}

void Search::uci_report() const {
    if (silent_)
        return;

    char output[512];
    char buf[32];
    char mpv_str[64];

    int64_t elapsed = timer::now() - limits_.start;
    uint64_t nps = stats_.nodes * 1000 / (elapsed + 1);

    float fhf = stats_.fail_high_first / (stats_.fail_high + 1.f);

    StateInfo si;
    TTEntry tte;
    Board b = root_;

    for (int i = 0; i < n_pvs_; ++i) {
        /* const RootMove &rm = rmp_.get_move(i); */
        const RootMove &rm = pv_moves_[i];

        score_to_str(buf, sizeof(buf), rm.score);
        snprintf(mpv_str, 32, "multipv %d ", i + 1);

        int n = snprintf(output, sizeof(output), 
                "info score %s depth %d seldepth %d %snodes %llu time %lld "
                "nps %llu fhf %.4f pv ",
                buf, stats_.id_detph, stats_.sel_depth, 
                uci_cfg_.multipv > 1 ? mpv_str : "", stats_.nodes, elapsed, nps, fhf);

        Move m = rm.move;
        b = root_;
        for (int k = 0; k < stats_.id_detph; ++k) {
            n += move_to_str(output + n, sizeof(output) - n, m);

            b = b.do_move(m, &si);
            if (!g_tt.probe(b.key(), tte))
                break;
            if (m = Move(tte.move16); !b.is_valid_move(m))
                break;
            output[n++] = ' ';
        }

        output[n] = 0;

        sync_cout() << output << '\n';
    }
}
