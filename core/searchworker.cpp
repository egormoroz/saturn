#include "searchworker.hpp"
#include "../cli.hpp"
#include "eval.hpp"
#include "../movepicker.hpp"
#include "../primitives/utility.hpp"
#include "../tree.hpp"
#include "../tt.hpp"
#include <algorithm>

namespace {

bool can_return_ttscore(const TTEntry &tte, 
    int &alpha, int beta, int depth)
{
    if (tte.depth8 < depth)
        return false;

    if (tte.bound8 == BOUND_EXACT) {
        alpha = tte.score16;
        return true;
    }
    if (tte.bound8 == BOUND_ALPHA && tte.score16 <= alpha)
        return true;
    if (tte.bound8 == BOUND_BETA && tte.score16 >= beta) {
        alpha = beta;
        return true;
    }

    return false;
}

} //namespace

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
    //there is no iterative deepening yet
    g_tree.clear();
    MovePicker mp(root_);

    Move best_move = MOVE_NONE;
    int best_score = -VALUE_MATE;
    Board bb;
    for (Move m = mp.next<false>(); m != MOVE_NONE; 
            m = mp.next<false>()) 
    {
        size_t ndx = g_tree.begin_node(m, -VALUE_MATE, VALUE_MATE,
                limits_.max_depth, 0);
        bb = root_.do_move(m);
        stack_.push(root_.key(), m);

        int score = -search(bb, -VALUE_MATE, VALUE_MATE, 
                limits_.max_depth - 1);

        stack_.pop();
        g_tree.end_node(ndx, score);


        if (score > best_score) {
            best_score = score;
            best_move = m;
        }
    }

    auto elapsed = timer::now() - limits_.start;
    uint64_t nps = stats_.nodes * 1000 / (elapsed + 1);

    sync_cout() << "info score " << Score{best_score}
        << " depth " << limits_.max_depth
        << " nodes " << stats_.nodes
        << " time " << elapsed
        << " nps " << nps
        << " pv " << best_move << '\n'
        << "bestmove " << best_move << '\n';
}

int SearchWorker::search(const Board &b, int alpha, 
        int beta, int depth) 
{
    check_time();
    if (!loop_.keep_going())
        return 0;

    if (b.half_moves() >= 100 
        || (!b.checkers() && b.is_material_draw())
        || stack_.is_repetition(b.half_moves()))
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
    if (stack_.capped())
        return eval(b);

    TTEntry tte;
    Move ttm = MOVE_NONE;
    if (g_tt.probe(b.key(), tte)) {
        if (can_return_ttscore(tte, alpha, beta, depth))
            return alpha;
        if (ttm = Move(tte.move16); !b.is_valid_move(ttm))
            ttm = MOVE_NONE;
    }

    /* if (!ttm && depth >= 5) */
    /*     search(b, alpha, beta, depth - 1); */

    Board bb;
    MovePicker mp(b, ttm);
    int best_score = -VALUE_MATE, moves_tried = 0,
        old_alpha = alpha;
    Move best_move = MOVE_NONE;
    for (Move m = mp.next<false>(); m != MOVE_NONE; 
            m = mp.next<false>()) 
    {
        ++moves_tried;
        size_t ndx = g_tree.begin_node(m, alpha, beta, 
                depth, stack_.height());
        bb = b.do_move(m);
        stack_.push(b.key(), m);

        g_tt.prefetch(b.key());
        int score = -search(bb, -beta, -alpha, depth - 1);

        stack_.pop();
        g_tree.end_node(ndx, score);

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

    if (!moves_tried) {
        if (b.checkers())
            return stack_.mated_score();
        return 0;
    }

    if (loop_.keep_going()) {
        Bound bound = BOUND_ALPHA;
        if (alpha >= beta) bound = BOUND_BETA;
        else if (alpha > old_alpha) bound = BOUND_EXACT;

        g_tt.store(TTEntry(b.key(), alpha, bound,
            depth, best_move, false));
    }

    return alpha;
}

template int SearchWorker::quiescence<true>(
        const Board &b, int alpha, int beta);
template int SearchWorker::quiescence<false>(
        const Board &b, int alpha, int beta);

template<bool with_evasions>
int SearchWorker::quiescence(const Board &b, 
        int alpha, int beta) 
{
    check_time();
    if (!loop_.keep_going() || b.half_moves() >= 100
        || b.is_material_draw()
        || stack_.is_repetition(b.half_moves()))
        return 0;

    if (stack_.capped())
        return eval(b);

    stats_.nodes++;
    stats_.qnodes++;

    //Mate distance pruning
    int mated_score = stack_.mated_score();
    alpha = std::max(alpha, mated_score);
    beta = std::min(beta, -mated_score - 1);
    if (alpha >= beta)
        return alpha;

    if constexpr (!with_evasions) {
        int stand_pat = eval(b);
        alpha = std::max(alpha, stand_pat);
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
        if (only_tacticals && !b.see_ge(m))
            continue;

        size_t ndx = g_tree.begin_node(m, alpha, beta, 
                0, stack_.height());
        bb = b.do_move(m);
        stack_.push(b.key(), m);

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

