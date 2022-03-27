#include "searchworker.hpp"
#include "../cli.hpp"
#include "eval.hpp"
#include "../movepicker.hpp"
#include "../primitives/utility.hpp"
#include "../tree.hpp"
#include "../tt.hpp"
#include <algorithm>
#include <sstream>
#include <cstring>
#include <cmath>

namespace {

constexpr bool DO_NMP = true;
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
}

void RootMovePicker::reset(const Board &root){
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

Move RootMovePicker::first() const {
    return num_moves_ ? moves_[0].move : MOVE_NONE;
}

Move RootMovePicker::next() {
    if (cur_ >= num_moves_)
        return MOVE_NONE;
    return moves_[cur_++].move;
}

void RootMovePicker::update_last(int score, uint64_t nodes) {
    assert(cur_ > 0 && cur_ <= num_moves_);
    auto &last = moves_[cur_ - 1];
    last.nodes = nodes;
    last.prev_score = last.score;
    last.score = score;
}

int RootMovePicker::num_moves() const {
    return num_moves_;
}

void RootMovePicker::complete_iter() {
    std::sort(moves_.begin(), moves_.begin() + num_moves_,
        [](const RootMove &x, const RootMove &y)
    {
        if (x.score != y.score) return x.score > y.score;
        return x.prev_score > y.prev_score;
    });
    cur_ = 0;
}

SearchWorker::SearchWorker() 
    : root_(Board::start_pos())
{
    loop_.start(std::bind(
            &SearchWorker::iterative_deepening, this));
}

void SearchWorker::go(const Board &root, const Stack &st, 
        const SearchLimits &limits)
{
    loop_.pause();
    loop_.wait_for_completion();

    root_ = root;
    stack_ = st;
    limits_ = limits;
    man_.start = limits.start;
    man_.max_time = limits_.move_time;
    stats_.reset();
    rmp_.reset(root_);
    hist_.reset();

    man_.init(limits, root.side_to_move(), st.total_height());

    memset(counters_.data(), 0, sizeof(counters_));
    memset(followups_.data(), 0, sizeof(followups_));

    loop_.resume();
}

void SearchWorker::stop() {
    loop_.pause();
}

void SearchWorker::wait_for_completion() {
    loop_.wait_for_completion();
}

void SearchWorker::check_time() {
    if (stats_.nodes & 2047)
        return;
    if (loop_.keep_going() && !limits_.infinite
            && man_.out_of_time())
        loop_.pause();
}

void SearchWorker::iterative_deepening() {
    Move pv[MAX_DEPTH]{};
    int pv_len = 0, score = 0, prev_score, ebf = 1;
    uint64_t prev_nodes, nodes = 0;
    std::ostringstream ss;

    if (rmp_.num_moves() == 1 || is_draw()) {
        sync_cout() << "bestmove " << rmp_.first() << '\n';
        return;
    }

    auto report = [&](int d) {
        auto elapsed = timer::now() - limits_.start;
        uint64_t nps = stats_.nodes * 1000 / (elapsed + 1);

        if (pv_len = g_tt.extract_pv(root_, pv, d); !pv_len) {
            pv_len = 1;
            pv[0] = rmp_.first();
        }

        ss.str("");
        ss.clear();
        float fhf = stats_.fail_high_first 
            / float(stats_.fail_high + 1);
        ss << "info score " << Score{score}
           << " depth " << d
           << " seldepth " << stats_.sel_depth
           << " nodes " << stats_.nodes
           << " time " << elapsed
           << " nps " << nps
           << " fhf " << fhf
           << " ebf " << ebf
           << " pv ";

        for (int i = 0; i < pv_len; ++i)
            ss << pv[i] << ' ';
        sync_cout() << ss.str() << '\n';
    };

    prev_nodes = 1;
    score = search_root(-VALUE_MATE, VALUE_MATE, 1);
    nodes = stats_.nodes;
    report(1);
    for (int d = 2; d <= limits_.max_depth; ++d) {
        g_tree.clear();
        prev_nodes = nodes;
        uint64_t before = stats_.nodes;
        prev_score = score;
        TimePoint start = timer::now();

        score = aspriration_window(score, d);
        if (!loop_.keep_going())
            break;
        report(d);

        nodes = stats_.nodes - before;
        ebf = static_cast<int>((nodes + prev_nodes - 1) 
                / std::max(uint64_t(1), prev_nodes));

        TimePoint now = timer::now(), 
              time_left = man_.start + man_.max_time - now;
        if (abs(score - prev_score) < 8 && !limits_.infinite
                && !limits_.move_time && now - start >= time_left)
            break; //assume we don't have enough time to go 1 ply deeper

        if (abs(score) >= VALUE_MATE - d)
            break;
    }
    sync_cout() << "bestmove " << pv[0] << '\n';
}

int SearchWorker::aspriration_window(int score, int depth) {
    if (depth <= 5)
        return search_root(-VALUE_MATE, VALUE_MATE, depth);

    int delta = 16, alpha = score - delta, 
        beta = score + delta;
    while (loop_.keep_going()) {
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

int SearchWorker::search_root(int alpha, int beta, int depth) {
    TTEntry tte;
    Move ttm = MOVE_NONE;
    if (g_tt.probe(root_.key(), tte)) {
        if (can_return_ttscore(tte, alpha, beta, depth, 0))
            return alpha;
        if (ttm = Move(tte.move16); !root_.is_valid_move(ttm))
            ttm = MOVE_NONE;
    }

    Move best_move = MOVE_NONE;
    int best_score = -VALUE_MATE, old_alpha = alpha,
        moves_tried = 0;
    Board bb;
    for (Move m = rmp_.next(); m != MOVE_NONE; m = rmp_.next()) {
        uint64_t nodes_before = stats_.nodes;
        size_t ndx = g_tree.begin_node(m, alpha, beta, depth - 1, 0);
        bb = root_.do_move(m);
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
    if (loop_.keep_going()) {
        g_tt.store(TTEntry(root_.key(), alpha, 
            determine_bound(alpha, beta, old_alpha),
            depth, best_move, 0, false));
    }

    return alpha;
}

int SearchWorker::search(const Board &b, int alpha, 
        int beta, int depth) 
{
    const int ply = stack_.height();
    const bool pv = alpha != beta - 1;

    check_time();
    if (!loop_.keep_going())
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

    int16_t eval = evaluate(b);
    bool improving = !b.checkers() && ply >= 2 
        && stack_.at(ply - 2).eval < eval;

    if (depth >= 4 && !ttm)
        --depth;

    if (pv || b.checkers())
        goto move_loop; //skip pruning

    //Reverse futility pruning
    if (depth < 7 && eval - 175 * depth / (1 + improving) >= beta
            && abs(beta) < MATE_BOUND)
        return eval;

    //Null move pruning
    if (DO_NMP && depth >= 3
        && b.plies_from_null() && !avoid_null
        && b.has_nonpawns(b.side_to_move())
        && eval >= beta)
    {
        int R = 3 + depth / 6, n_depth = depth - R - 1;
        size_t ndx = g_tree.begin_node(MOVE_NULL, alpha, 
                beta, n_depth, ply, NodeType::Null);
        stack_.push(b.key(), MOVE_NULL, eval);

        int score = -search(b.do_null_move(), -beta, 
                -beta + 1, n_depth);

        stack_.pop();
        g_tree.end_node(ndx, score);

        if (score >= beta)
            return beta;

        avoid_null = true;
    }

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

    Board bb;
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
        bb = b.do_move(m);

        if (bb.checkers() && b.see_ge(m))
            new_depth++;

        int lmp_threshold = (3 + 2 * depth * depth) / (2 - improving);
        if (!pv && !bb.checkers() && is_quiet
                && moves_tried > lmp_threshold) 
            break;

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

    if (loop_.keep_going()) {
        g_tt.store(TTEntry(b.key(), alpha, 
            determine_bound(alpha, beta, old_alpha),
            depth, best_move, ply, avoid_null));
    }

    return alpha;
}

template<bool with_evasions>
int SearchWorker::quiescence(const Board &b, 
        int alpha, int beta) 
{
    check_time();
    if (!loop_.keep_going() || b.half_moves() >= 100
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

    MovePicker mp(b);
    Board bb;
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
        bb = b.do_move(m);
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

bool SearchWorker::is_draw() const {
    if (root_.half_moves() >= 100 
        || (!root_.checkers() && root_.is_material_draw())
        || stack_.is_repetition(root_))
        return true;
    return false;
}

