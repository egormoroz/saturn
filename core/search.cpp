#include <algorithm>
#include <cstring>
#include <cmath>

#include "search.hpp"
#include "eval.hpp"
#include "../movepicker.hpp"
#include "../primitives/utility.hpp"
#include "../tt.hpp"
#include "../nnue/evaluate.hpp"
#include "../nnue/nnue_state.hpp"
#include "../scout.hpp"


template<bool is_root>
struct AutoMovePicker {
    template<bool>
    Move next();
    Stage stage() const;
    int num_excluded_moves() const;
    void complete_iter(int best_move_idx);
};

template<>
struct AutoMovePicker<true> {
    AutoMovePicker(
        RootMovePicker &rmp,
        const Board &board, Move ttm,
        const Move *killers = nullptr,
        const Histories *histories = nullptr,
        Move counter = MOVE_NONE,
        Move followup = MOVE_NONE)
            : rmp_(rmp)
            
    {
        (void)(board);
        (void)(ttm);
        (void)(killers);
        (void)(histories);
        (void)(counter);
        (void)(followup);
    }

    template<bool b>
    Move next() { return rmp_.next(); }

    // dummy 
    Stage stage() const { return Stage::TT_MOVE; }

    int num_excluded_moves() const { return rmp_.num_excluded_moves(); }
    void complete_iter(int best_move_idx) { rmp_.complete_iter(best_move_idx); }

private:
    RootMovePicker &rmp_;
};

template<>
struct AutoMovePicker<false> : public MovePicker {
    AutoMovePicker(
        RootMovePicker &rmp,
        const Board &board, Move ttm,
        const Move *killers = nullptr,
        const Histories *histories = nullptr,
        Move counter = MOVE_NONE,
        Move followup = MOVE_NONE)
            : MovePicker(board, ttm, killers, histories, counter, followup)
    {
        (void)(rmp);
    }

    int num_excluded_moves() const { return 0; }
    void complete_iter(int best_move_idx) { (void)best_move_idx; }
};

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

void init_reduction_tables(float k) {
    // k == 0.65 *could be* like +5 elo
    // TODO: test this
    for (int depth = 1; depth < 32; ++depth)
        for (int moves = 1; moves < 64; ++moves)
            LMR[depth][moves] = static_cast<uint8_t>(
                0.1 + log(depth) * log(moves) * k
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
    : root_(Board::start_pos(&root_si_)),
      counters_({MOVE_NONE}),
      followups_({MOVE_NONE})
{
}

void Search::set_silent(bool s) {
    silent_ = s;
}

void Search::setup(const Board &root,  const SearchLimits &limits,
        UCISearchConfig usc, const Stack *st, bool ponder)
{
    // should we really do this?
    g_tt.new_search();

    pondering_ = ponder;
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

    man_.init(limits, root.side_to_move(), usc.move_overhead);

    memset(counters_.data(), 0, sizeof(counters_));
    memset(followups_.data(), 0, sizeof(followups_));

    keep_going_ = true;
}

bool Search::keep_going() {
    if (stats_.nodes % 2048 == 0 && keep_going_) {
        keep_going_ = pondering_ || limits_.infinite || !man_.out_of_time()
            || stats_.id_depth <= limits_.min_depth;
        if (limits_.max_nodes)
            keep_going_ = keep_going_ && stats_.nodes < limits_.max_nodes;
    }
    return keep_going_;
}

void Search::iterative_deepening() {
    int score = 0, prev_score;
    n_pvs_ = std::min(uci_cfg_.multipv, rmp_.num_moves());

    if (!n_pvs_){ 
        if (!silent_)
            sync_cout() << "no legal moves\n";
        n_pvs_ = 0;
    }

    if ((rmp_.num_moves() == 1/* || is_board_drawn(root_)*/) && !silent_ && !pondering_) {
        RootMove m = rmp_.best_move();
        sync_cout() << "bestmove " << m.move << '\n';
        return;
    }

    stats_.id_depth = 1;
    rmp_.mpv_reset();
    for (int i = 0; i < n_pvs_; ++i) {
        score = search<true>(root_, -VALUE_MATE, VALUE_MATE, 1);
        rmp_.exclude_top_move(score);
    }
    extract_pvmoves();
    uci_report();


    for (int d = 2; d <= limits_.max_depth; ++d) {
        stats_.id_depth = d;
        prev_score = score;
        TimePoint start = timer::now();

        rmp_.mpv_reset();
        for (int i = 0; i < n_pvs_; ++i) {
            score = aspiration_window(score, d);
            if (!keep_going())
                break;

            rmp_.exclude_top_move(score);
        }

        if (!keep_going())
            break;

        assert(rmp_.num_excluded_moves() == n_pvs_);
        extract_pvmoves();
        uci_report();

        if (limits_.infinite || pondering_)
            continue;

        TimePoint now = timer::now(), 
              time_left = man_.start + man_.max_time - now;
        if (abs(score - prev_score) < 8 && !limits_.move_time 
                && now - start >= time_left && d >= limits_.min_depth)
            break; //assume we don't have enough time to go 1 ply deeper

        if (uci_cfg_.multipv == 1 && abs(score) >= VALUE_MATE - d)
            break;
    }

    while (pondering_);

    RootMove rm = rmp_.best_move();
    if (!silent_) {
        auto out = sync_cout();
        out << "bestmove " << rm.move;

        TTEntry tte;
        Board bb = root_.do_move(rm.move);
        if (g_tt.probe(bb.key(), tte) && bb.is_valid_move(Move(tte.move16)))
            out << " ponder " << Move(tte.move16);
        out << '\n';
    }
}

void Search::atomic_stop() {
    keep_going_ = false;
    pondering_ = false;
}

void Search::stop_pondering() {
    pondering_ = false;
}

RootMove Search::get_pv_start(int i) const { 
    assert(i < n_pvs_);
    return pv_moves_[i]; 
}

int Search::num_pvs() const { return n_pvs_; }

int Search::aspiration_window(int score, int depth) {
    if (depth < uci_cfg_.asp_min_depth)
        return search<true>(root_, -VALUE_MATE, VALUE_MATE, depth);

    int delta = uci_cfg_.asp_init_delta, alpha = score - delta, 
        beta = score + delta;
    while (keep_going()) {
        if (alpha <= -3000) alpha = -VALUE_MATE;
        if (beta >= 3000) beta = VALUE_MATE;

        score = search<true>(root_, alpha, beta, depth);

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

template int Search::search<true>(const Board &b, int alpha, int beta, int depth);
template int Search::search<false>(const Board &b, int alpha, int beta, int depth);

template<bool is_root>
int Search::search(const Board &b, int alpha, 
        int beta, int depth) 
{
    const int ply = stack_.height();
    const bool pv_node = alpha != beta - 1;
    // Used for Singular Extension. Excluded move is the TT move.
    const Move excluded = ply > 0 ? stack_.at(ply).excluded : MOVE_NONE;

    if (!keep_going())
        return 0;

    //Mate distance pruning
    if (!is_root) {
        int mated_score = stack_.mated_score();
        alpha = std::max(alpha, mated_score);
        beta = std::min(beta, -mated_score - 1);
        if (alpha >= beta)
            return alpha;

        if (is_board_drawn(b))
            return 0;
    }

    if (depth <= 0)
        return b.checkers() ? quiescence<true>(b, alpha, beta)
            : quiescence<false>(b, alpha, beta);

    stats_.nodes++;
    stats_.sel_depth = std::max(stats_.sel_depth, ply);

    auto &entry = stack_.at(ply);
    g_tt.prefetch(b.key());

    TTEntry tte;
    bool avoid_null = false;
    Move ttm = MOVE_NONE;
    int16_t eval;
    if (g_tt.probe(b.key(), tte)) {
        if (ttm = Move(tte.move16); !b.is_valid_move(ttm))
            ttm = MOVE_NONE;
        
        // TODO: consider not returning when in PV node
        if (!is_root && !excluded && can_return_ttscore(tte, alpha, beta, depth, ply)) {
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
    bool improving = !b.checkers() && ply >= 2 && stack_.at(ply - 2).eval < eval;

    if (depth >= 4 && !ttm)
        --depth;

    // TODO: check if forward pruning makes sense in a singularity search
    if (pv_node || b.checkers() || excluded)
        goto move_loop; //skip pruning

    //Reverse futility pruning
    if (depth < 7 && eval - 175 * depth / (1 + improving) >= beta
            && abs(beta) < MATE_BOUND)
        return eval;

    // Razoring
    if (!b.checkers() && depth < 6 && eval + 200 * depth <= alpha) {
        if (quiescence<false>(b, alpha, beta) <= alpha)
            return alpha;
    }

    //Null move pruning
    if (depth >= 3 && !excluded
        && b.plies_from_null() && !avoid_null
        && b.has_nonpawns(b.side_to_move())
        && eval >= beta)
    {
        int R = 3 + depth / 6, n_depth = depth - R - 1;
        stack_.push(b.key(), MOVE_NULL, eval);

        int score = -search(b.do_null_move(&si), -beta, 
                -beta + 1, n_depth);

        stack_.pop();

        if (score >= beta)
            return beta;

        avoid_null = true;
    }

move_loop:
    Move opp_move = is_root ? MOVE_NONE : stack_.at(ply - 1).move,
         prev = MOVE_NONE, followup = MOVE_NONE,
         counter = is_root ? MOVE_NONE : counters_[from_to(opp_move)];
    if (ply >= 2) {
        prev = stack_.at(ply - 2).move;
        followup = followups_[from_to(prev)];
    }

    AutoMovePicker<is_root> amp(rmp_, b, ttm, entry.killers, &hist_, counter, followup);

    Board bb(&si);
    // auto search_move = [&](int depth, bool zw) {
    //     int t_beta = zw ? -(alpha + 1) : -beta;
    //     int score = -search(bb, t_beta, -alpha, depth);
    //     return score;
    // };

    std::array<Move, 64> quiets;
    int num_quiets{};
    int best_score = -VALUE_MATE, moves_tried = 0,
        old_alpha = alpha, score = 0, best_move_idx = 0;
    Move best_move = MOVE_NONE;
    for (Move m = amp.template next<false>(); m != MOVE_NONE; 
            m = amp.template next<false>()) 
    {
        if (m == excluded) continue;

        bool is_quiet = b.is_quiet(m);
        int new_depth = depth - 1, r = 0;
        bool killer_or_counter = m == counter
            || entry.killers[0] == m || entry.killers[1] == m;
        bb = b.do_move(m, &si);

        // Check extension
        int extension = 0;
        if (bb.checkers() && b.see_ge(m)) 
            extension = 1;

        // Singular move extension
        // Extend if TT move is so good it causes a beta cutoff, whereas all other moves don't.
        if (!is_root && m == ttm && !excluded && depth >= 8 
                && tte.depth5 >= depth - 3 && tte.bound2 & BOUND_BETA) 
        {
            int rbeta = tte.score16 - depth;

            entry.excluded = ttm;
            int score = search<false>(b, rbeta - 1, rbeta, (depth - 1) / 2);
            entry.excluded = MOVE_NONE;

            if (score < rbeta - 16)
                extension += 2;
            else if (score < rbeta)
                extension += 1;
            // Without these reductions SE loses elo
            else if (tte.score16 >= beta)
                extension -= 1;
            else if (tte.score16 <= old_alpha)
                extension -= 1;
        }

        extension = std::min(extension, 2);
        new_depth += is_root ? 0 : extension;

        int lmp_threshold = (3 + 2 * depth * depth) / (2 - improving);
        if (!pv_node && !bb.checkers() && is_quiet && moves_tried > lmp_threshold) 
            break;

        // Late more reductions
        if (depth > 2 && moves_tried > 1 && is_quiet) {
            r = LMR[std::min(31, depth)][std::min(63, moves_tried)];
            if (!pv_node) ++r;
            if (!improving) ++r;
            if (killer_or_counter) r -= 2;

            // We are implicitly kind of double-extending SEE>0 checks... Is it a good idea?
            // I don't have the hardware to test it for like 8k games, 
            // but a few hundred @ 10+0.1 show it doesn't lose any elo
            //if (bb.checkers()) --r; 

            r -= hist_.get_score(b, m) / 8192;

            r = std::clamp(r, 0, new_depth - 1);
            new_depth -= r;
        }

        stack_.push(b.key(), m, eval);

        //Zero-window search
        if (!pv_node || moves_tried)
            score = -search(bb, -alpha - 1, -alpha, new_depth);
            // score = search_move(new_depth, true);

        //Re-search if reduced move beats alpha
        if (r && score > alpha) {
            new_depth += r;
            score = -search(bb, -alpha - 1, -alpha, new_depth);
            // score = search_move(new_depth, true);
        }

        //(Re-)search with full window
        if (pv_node && ((score > alpha && score < beta) || !moves_tried))
            score = -search(bb, -beta, -alpha, new_depth);
            // score = search_move(new_depth, false);

        stack_.pop();
        ++moves_tried;

        if (!keep_going())
            return 0;

        if (score > best_score) {
            best_score = score;
            best_move = m;
            best_move_idx = moves_tried - 1;
        }

        if (b.is_quiet(m) && num_quiets < 64)
            quiets[num_quiets++] = m;

        if (score > alpha)
            alpha = score;
        if (score >= beta)
            break;
    }

    if (!moves_tried && !excluded) {
        if (b.checkers())
            return stack_.mated_score();
        return 0; //stalemate
    }

    // remember the move that cuased beta cutoff
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

    if (!excluded) {
        if (!is_root || (is_root && amp.num_excluded_moves() == 0)) {
            g_tt.store(b.key(), alpha, eval,
                determine_bound(alpha, beta, old_alpha),
                depth, best_move, ply, avoid_null);
        }

        amp.complete_iter(best_move_idx);
    }

    return alpha;
}

template<bool with_evasions>
int Search::quiescence(const Board &b, 
        int alpha, int beta) 
{
    if (!keep_going() || is_board_drawn(b))
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

        bb = b.do_move(m, &si);
        stack_.push(b.key(), m, eval);

        //filter out perpetual checks
        bool gen_evasions = !with_evasions && bb.checkers();
        int score = gen_evasions ? -quiescence<true>(bb, -beta, -alpha)
            : -quiescence<false>(bb, -beta, -alpha);

        stack_.pop();

        if (score > alpha)
            alpha = score;
        if (score >= beta) 
            return beta;
    }

    if (with_evasions && !moves_tried)
        return stack_.mated_score();

    return alpha;
}

bool Search::is_board_drawn(const Board &b) const {
    if (b.half_moves() >= 100 
        || (!b.checkers() && b.is_material_draw())
        || stack_.is_repetition(b))
        return true;
    return false;
}

int16_t Search::evaluate(const Board &b) {
    /* int simple_eval = material_advantage(b); */
    /* if (abs(simple_eval) > 2000) */
    /*     return simple_eval; */

    int16_t result;
    if (!ev_cache_.probe(b.key(), result)) {
        result = static_cast<int16_t>(nnue::evaluate(b));
        ev_cache_.store(b.key(), result);
    }

    return result;
}

void Search::extract_pvmoves() {
    assert(n_pvs_ == rmp_.num_excluded_moves());
    for (int i = 0; i < n_pvs_; ++i)
        pv_moves_[i] = rmp_.get_move(i);
    std::stable_sort(pv_moves_, pv_moves_ + n_pvs_,
        [](const RootMove &x, const RootMove &y) { return x.score > y.score; });
}

void Search::uci_report() const {
    if (silent_)
        return;

    char output[512];
    char buf[32];
    char mpv_str[64];

    long long elapsed = timer::now() - limits_.start;
    unsigned long long nps = stats_.nodes * 1000 / (elapsed + 1);

    float fhf = stats_.fail_high_first / (stats_.fail_high + 1.f);

    StateInfo si;
    TTEntry tte;
    Board b = root_;

    for (int i = 0; i < n_pvs_; ++i) {
        const RootMove &rm = pv_moves_[i];

        score_to_str(buf, sizeof(buf), rm.score);
        snprintf(mpv_str, 32, "multipv %d ", i + 1);

        int n = snprintf(output, sizeof(output), 
                "info score %s depth %d seldepth %d %snodes %llu time %lld "
                "nps %llu fhf %.4f pv ",
                buf, stats_.id_depth, stats_.sel_depth, 
                uci_cfg_.multipv > 1 ? mpv_str : "", 
                (unsigned long long)stats_.nodes, elapsed, nps, fhf);

        Move m = rm.move;
        b = root_;
        for (int k = 0; k < stats_.id_depth; ++k) {
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
