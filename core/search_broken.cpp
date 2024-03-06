#include <algorithm>
#include <cstring>
#include <cmath>
#include <numeric>

#include "search_broken.hpp"
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
    if (tte.depth8 < depth)
        return false;

    int tt_score = tte.score(ply);
    if (tte.bound8 == BOUND_EXACT) {
        alpha = tt_score;
        return true;
    }
    if (tte.bound8 == BOUND_ALPHA && tt_score <= alpha)
        return true;
    if (tte.bound8 == BOUND_BETA && tt_score >= beta) {
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

void RMP::reset(const Board &root) {
    start_ = 0;

    Move ttm = MOVE_NONE;
    TTEntry tte;
    if (g_tt.probe(root.key(), tte)) {
        if (!root.is_valid_move(ttm = Move(tte.move16)))
            ttm = MOVE_NONE;
    }

    MovePicker mp(root, ttm);
    cur_ = num_moves_ = 0;
    for (Move m = mp.next<false>(); m != MOVE_NONE; 
            m = mp.next<false>())
    {
        moves_[num_moves_++] = { m, 0, 0, 0 };
    }
}

const RootMove& RMP::first_move() const {
    assert(num_moves_ > 0);
    return moves_[0];
}

Move RMP::next() {
    if (cur_ >= num_moves_)
        return MOVE_NONE;
    return moves_[cur_++].move;
}

void RMP::update_last(int score, uint64_t nodes) {
    assert(cur_ > 0 && cur_ <= num_moves_);
    auto &last = moves_[cur_ - 1];
    last.nodes = nodes;
    last.prev_score = last.score;
    last.score = score;
}

void RMP::extract_pv(const Board &root, PVLine &pv, int max_len) {
    assert(start_ > 0 && start_ <= num_moves_);
    const RootMove &rm = moves_[start_ - 1];

    g_tt.extract_pv(root, pv, max_len, rm.move);
    pv.score = rm.score;
}


int RMP::num_moves() const { return num_moves_; }

void RMP::complete_iter() {
    assert(start_ <= num_moves_);
    for (int i = cur_; i < num_moves_; ++i) {
        moves_[i].prev_score = moves_[i].score;
        moves_[i].score = -VALUE_MATE;
        moves_[i].nodes = 0;
    }

    std::stable_sort(moves_.begin() + start_, moves_.begin() + num_moves_,
        [](const RootMove &x, const RootMove &y)
    {
        if (x.score != y.score) return x.score > y.score;
        return x.prev_score > y.prev_score;
    });
    
    cur_ = start_;
}

void RMP::complete_iter_ttcutoff(Move ttm, int score) {
    int ttm_idx = -1;
    for (int i = start_; i < num_moves_; ++i) {
        if (moves_[i].move == ttm) {
            ttm_idx = i;
            break;
        }
    }
    assert(ttm_idx >= 0);
    RootMove & rm = moves_[ttm_idx];
    rm.nodes = 0;
    rm.prev_score = rm.score;
    rm.score = score;

    // bring the best move to the top
    for (int i = ttm_idx; i > start_; --i)
        std::swap(moves_[i], moves_[i - 1]);

    cur_ = start_;
}

void RMP::new_id_iter() {
    start_ = 0;
    cur_ = 0;


    std::stable_sort(moves_.begin(), moves_.begin() + num_moves_,
        [](const RootMove &x, const RootMove &y)
    {
        if (x.score != y.score) return x.score > y.score;
        return x.prev_score > y.prev_score;
    });
}

void RMP::complete_pvline() {
    ++start_;
    cur_ = start_;
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
        const Stack *st)
{
    // should we really do this?
    // g_tt.new_search();

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
    }
    return keep_going_;
}

void Search::atomic_stop() {
    keep_going_ = false;
}

int Search::iterative_deepening(int multipv) {
    // TODO: handle the 'no legal moves' situation
    if (rmp_.num_moves() == 0)
        return 0;

    if (rmp_.num_moves() == 1) {
        Move m = rmp_.first_move().move;
        pvs_[0].len = 1;
        pvs_[0].moves[0] = m;

        TTEntry tte;
        if (g_tt.probe(root_.key(), tte) && tte.move16 == m)
            pvs_[0].score = tte.score16;
        else
            pvs_[0].score = 0;

        if (!silent_) {
            uci_report(1);
            sync_cout() << "bestmove " << m << '\n';
        }

        return 1;
    }

    multipv = std::min(rmp_.num_moves(), multipv);

    stats_.id_detph = 1;
    int score = search_root(-VALUE_MATE, VALUE_MATE, 1);
    rmp_.complete_pvline();
    rmp_.extract_pv(root_, pvs_[0], 1);
    if (!silent_)
        uci_report(1);

    mpv_search_ = multipv > 1;
    for (int depth = 2; depth <= limits_.max_depth; ++depth) {
        g_tree.clear();
        stats_.id_detph = depth;

        int prev_score = score;
        TimePoint start = timer::now();

        rmp_.new_id_iter();
        for (int i = 0; i < multipv; ++i) {
            score = aspriration_window(score, depth);
            if (!keep_going())
                break;
            rmp_.complete_pvline();

            int pv_len = abs(score) > MATE_BOUND
                ? VALUE_MATE - abs(score) + 1
                : depth;
            rmp_.extract_pv(root_, pvs_[i], pv_len);
        }

        if (!keep_going())
            break;

        if (!silent_)
            uci_report(multipv);

        TimePoint now = timer::now(); 
        TimePoint time_left = man_.start + man_.max_time - now;
        if (abs(score - prev_score) < 8 && !limits_.infinite
                && !limits_.move_time && now - start >= time_left)
            break; //assume we don't have enough time to go 1 ply deeper

        if (abs(score) >= VALUE_MATE - depth)
            break;
    }

    if (!silent_)
        sync_cout() << "bestmove " << rmp_.first_move().move << '\n';

    return multipv;
}

const MultiPV& Search::get_pvs() const { return pvs_; }

int Search::aspriration_window(int score, int depth) {
    if (depth <= 5)
        return search_root(-VALUE_MATE, VALUE_MATE, depth);

    int delta = 16, alpha = score - delta, 
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
    TTEntry tte;
    Move ttm = MOVE_NONE;
    if (g_tt.probe(root_.key(), tte)) {
        ttm = Move(tte.move16);
        if (can_return_ttscore(tte, alpha, beta, depth, 0)
                && root_.is_valid_move(ttm))
        {
            rmp_.complete_iter_ttcutoff(ttm, alpha);
            return alpha;
        }
        ttm = MOVE_NONE;
    }

    Move best_move = MOVE_NONE;
    int best_score = -VALUE_MATE, old_alpha = alpha,
        moves_tried = 0;
    StateInfo si;
    Board bb(&si);
    for (Move m = rmp_.next(); m != MOVE_NONE; m = rmp_.next()) {
        uint64_t nodes_before = stats_.nodes;
        size_t ndx = g_tree.begin_node(m, alpha, beta, depth - 1, 0);
        bb = root_.do_move(m, &si);
        stack_.push(root_.key(), m);

        int score;
        if (!moves_tried) {
            score = -search(bb, -beta, -alpha, depth - 1);
        } else {
            score = -search(bb, -(alpha + 1), -alpha, depth - 1);
            if (score > alpha && score < beta)
                score = -search(bb, -beta, -alpha, depth - 1);
        }

        ++moves_tried;
        stack_.pop();
        g_tree.end_node(ndx, score);
        rmp_.update_last(score, stats_.nodes - nodes_before);

        if (score > best_score) {
            best_score = score;
            best_move = m;
        }

        if (score > alpha)
            alpha = score;
        if (score >= beta) {
            alpha = beta;
            break;
        }
    }
    
    rmp_.complete_iter();

    if (!mpv_search_ && keep_going()) {
        g_tt.store(TTEntry(root_.key(), alpha, 
            determine_bound(alpha, beta, old_alpha),
            depth, best_move, 0, false));
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
    }

    StateInfo si;
    int16_t eval = evaluate(b);
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
        g_tt.store(TTEntry(b.key(), alpha, 
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

int16_t Search::evaluate(const Board &b) const {
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

void Search::uci_report(int n_pvs) const {
    assert(n_pvs <= MAX_MOVES);
    uint8_t sorted_ics[MAX_MOVES]{};

    if (n_pvs > 1) {
        std::iota(sorted_ics, sorted_ics+MAX_MOVES, 0);
        std::sort(sorted_ics, sorted_ics + n_pvs, [&](uint8_t i, uint8_t j) {
            return pvs_[i].score > pvs_[j].score;
        });
    }


    char output[512];
    char buf[16];
    char mpv_str[32];

    auto elapsed = timer::now() - limits_.start;
    uint64_t nps = stats_.nodes * 1000 / (elapsed + 1);

    float fhf = stats_.fail_high_first / (stats_.fail_high + 1.f);

    for (int i = 0; i < n_pvs; ++i) {
        const PVLine &pv = pvs_[sorted_ics[i]];

        score_to_str(buf, sizeof(buf), pv.score);

        snprintf(mpv_str, 32, "multipv %d ", i + 1);

        int n = snprintf(output, sizeof(output), 
                "info score %s depth %d seldepth %d %snodes %llu time %lld "
                "nps %llu fhf %.4f pv ",
                buf, stats_.id_detph, stats_.sel_depth, 
                mpv_search_ ? mpv_str : "", stats_.nodes, elapsed, nps, fhf);

        for (int k = 0; k < pv.len; ++k) {
            n += move_to_str(output + n, sizeof(output) - n, pv.moves[k]);
            output[n++] = ' ';
        }

        output[n] = 0;

        sync_cout() << output << '\n';
    }
}

