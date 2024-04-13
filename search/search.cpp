#include <algorithm>
#include <cstring>
#include <cmath>
#include <iomanip>

#include "search.hpp"
#include "../movepicker.hpp"
#include "../primitives/utility.hpp"
#include "../tt.hpp"
#include "../mininnue/nnue.hpp"
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

int16_t move_value(const Board &b, Move m) {
    if (type_of(m) == EN_PASSANT)
        return mg_value[PAWN];

    int16_t value = 0;
    if (type_of(m) == PROMOTION)
        value = mg_value[prom_type(m)] - mg_value[PAWN];

    return value + mg_value[type_of(b.piece_on(to_sq(m)))];
}

} //namespace

void update_reduction_tables() {
    const float fk = params::lmr_coeff / 100.f;
    for (int depth = 1; depth < 32; ++depth)
        for (int moves = 1; moves < 64; ++moves)
            LMR[depth][moves] = static_cast<uint8_t>(
                0.1 + log(depth) * log(moves) * fk
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

void Search::new_game() {
    g_tt.new_search();
    hist_.reset();
    memset(counters_.data(), 0, sizeof(counters_));
    memset(followups_.data(), 0, sizeof(followups_));
}

void Search::setup(const Board &root,  const SearchLimits &limits,
        const Stack *st, bool ponder, int multipv)
{
    pondering_ = ponder;
    root_ = root;
    limits_ = limits;
    stats_.reset();

    rmp_.reset(root_);
    n_pvs_ = std::min(rmp_.num_moves(), multipv);

    if (st)
        stack_ = *st;
    else
        stack_.reset();

    root_.set_stateinfo(&root_si_);
    root_si_.previous = nullptr;
    mini::refresh_accumulator(root_, root_si_.acc, WHITE);
    mini::refresh_accumulator(root_, root_si_.acc, BLACK);

    man_.init(limits, root.side_to_move());

    keep_going_ = true;
}

bool Search::keep_going() {
    if (stats_.nodes % 2048 == 0 && keep_going_) {
        switch (limits_.type) {
        case SearchLimits::UNLIMITED: 
        case SearchLimits::DEPTH:
            break;

        case SearchLimits::NODES:
            keep_going_ = stats_.nodes < limits_.nodes;
            break;
        case SearchLimits::TIME:
            keep_going_ = !man_.out_of_time();
            break;
        };
    }
    return keep_going_;
}

void Search::iterative_deepening() {
    int score = 0, prev_score;

    if (!n_pvs_){ 
        if (!silent_)
            sync_cout() << "no legal moves\n";
        n_pvs_ = 0;
    }

    if (rmp_.num_moves() == 1 && !silent_ && !pondering_) {
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


    const int max_depth = limits_.type == limits_.DEPTH ? limits_.depth : MAX_DEPTH;
    for (int d = 2; d <= max_depth; ++d) {
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

        if (limits_.type != limits_.TIME)
            continue;

        TimePoint now = timer::now(), 
              time_left = man_.start + man_.max_time - now;

        // TODO: we could try looking at EBF instead
        if (abs(score - prev_score) < 8 && !limits_.move_time 
                && now - start >= time_left && d >= limits_.depth)
            break; //assume we don't have enough time to go 1 ply deeper

        if (n_pvs_ == 1 && abs(score) >= VALUE_MATE - d)
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

const SearchStats& Search::get_stats() const { return stats_; }

int Search::aspiration_window(int score, int depth) {
    if (depth < params::asp_min_depth)
        return search<true>(root_, -VALUE_MATE, VALUE_MATE, depth);

    int delta = params::asp_init_delta, alpha = score - delta, 
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
    const bool is_pv = alpha != beta - 1;
    // Used for Singular Extension. Excluded move is the TT move.
    const Move excluded = ply > 0 ? stack_.at(ply).excluded : MOVE_NONE;
    
    const int see_margin[2] = {
        -20 * depth * depth,
        -64 * depth,
    };

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
        if (!is_root && !excluded && can_return_ttscore(tte, alpha, beta, depth, ply) && !is_pv) 
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

    if (stack_.capped()) return eval;

    StateInfo si;
    bool improving = !b.checkers() && ply >= 2 && stack_.at(ply - 2).eval < eval;

    if (depth >= params::iir_min_depth && !ttm)
        --depth;

    // TODO: check if forward pruning makes sense in a singularity search
    if (is_pv || b.checkers() || excluded)
        goto move_loop; //skip pruning

    //Reverse futility pruning
    if (depth <= params::rfp_max_depth 
            && eval - params::rfp_margin * depth / (1 + improving) >= beta
            && abs(beta) < MATE_BOUND)
        return eval;

    // Razoring
    if (!b.checkers() && depth <= params::rz_max_depth 
            && eval + params::rz_margin * depth <= alpha
            && quiescence<false>(b, alpha, beta) <= alpha) 
        return alpha;

    //Null move pruning
    if (depth >= params::nmp_min_depth && !excluded
        && b.plies_from_null() && !avoid_null
        && b.has_nonpawns(b.side_to_move())
        && eval >= beta)
    {
        int R = params::nmp_base + depth / params::nmp_depth_div 
            + std::min(2, (eval - beta) / params::nmp_eval_div);

        int n_depth = depth - R;
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
        bool killer_or_counter = m == counter
            || entry.killers[0] == m || entry.killers[1] == m;


        bb = b.do_move(m, &si);

        // Check extension
        int extension = 0;
        if (bb.checkers() && b.see_ge(m)) 
            extension = 1;

        // Singular move extension
        // Extend if TT move is so good it causes a beta cutoff, whereas all other moves don't.
        if (!is_root && m == ttm && !excluded && depth >= params::sing_min_depth 
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

        int new_depth = depth - 1, r = 0;
        new_depth += is_root ? 0 : extension;

        int lmp_threshold = (3 + 2 * depth * depth) / (2 - improving);
        if (!is_pv && !bb.checkers() && is_quiet && moves_tried > lmp_threshold) 
            break;

        // SEE pruning
        if (amp.stage() >= Stage::BAD_TACTICAL && depth <= params::seefp_depth 
                && !b.see_ge(m, see_margin[is_quiet]))
            continue;

        // Late more reductions
        if (depth > 2 && moves_tried > 1 && is_quiet) {
            r = LMR[std::min(31, depth)][std::min(63, moves_tried)];
            if (!is_pv) ++r;
            if (!improving) ++r;
            if (killer_or_counter) r -= 2;

            // We are implicitly kind of double-extending SEE>0 checks... Is it a good idea?
            // I don't have the hardware to test it for like 8k games, 
            // but a few hundred @ 10+0.1 show it doesn't lose any elo
            //if (bb.checkers()) --r; 

            r -= hist_.get_score(b, m) / params::lmr_hist_div;

            r = std::clamp(r, 0, new_depth - 1);
            new_depth -= r;
        }

        stack_.push(b.key(), m, eval);

        //Zero-window search
        if (!is_pv || moves_tried)
            score = -search(bb, -alpha - 1, -alpha, new_depth);

        //Re-search if reduced move beats alpha
        if (r && score > alpha) {
            new_depth += r;
            score = -search(bb, -alpha - 1, -alpha, new_depth);
        }

        //(Re-)search with full window
        if (is_pv && ((score > alpha && score < beta) || !moves_tried))
            score = -search(bb, -beta, -alpha, new_depth);

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

    if (stack_.capped()) return eval;

    StateInfo si;
    MovePicker mp(b);
    Board bb(&si);
    constexpr bool only_tacticals = !with_evasions;
    int moves_tried = 0;
    for (Move m = mp.next<only_tacticals>(); m != MOVE_NONE; 
            m = mp.next<only_tacticals>(), ++moves_tried)
    {
        // If we aren't evading a check, and we assume that the capture/promotion is free
        // but it still fails to raise alpha, we prune it
        if (!with_evasions && eval + move_value(b, m) + params::delta_margin <= alpha)
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
    int16_t result;
    if (!ev_cache_.probe(b.key(), result)) {
        result = static_cast<int16_t>(mini::evaluate(b));
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

    long long elapsed = timer::now() - limits_.start;
    unsigned long long nps = stats_.nodes * 1000 / (elapsed + 1);
    float fhf = stats_.fail_high_first / (stats_.fail_high + 1.f);

    auto out = sync_cout();

    for (int i = 0; i < n_pvs_; ++i) {
        const RootMove &rm = pv_moves_[i];

        out << "info multipv " << (i + 1)
            << " score " << Score { rm.score }
            << " depth " << stats_.id_depth
            << " seldepth " << stats_.sel_depth
            << " nodes " << stats_.nodes
            << " time " << elapsed
            << " nps " << nps
            << " fhf " << std::setprecision(4) << fhf
            << " pv ";

        Board b = root_;
        TTEntry tte;
        Move m = rm.move;
        for (int k = 0; k < stats_.id_depth; ++k) {
            out << m << ' ';

            b = b.do_move(m);
            if (g_tt.probe(b.key(), tte) && b.is_valid_move(Move(tte.move16)))
                m = Move(tte.move16);
            else
                break;
        }

        out << '\n';
    }
}
