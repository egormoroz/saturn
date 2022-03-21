#include "searchworker.hpp"
#include <cstdio>
#include "../cli.hpp"
#include "eval.hpp"
#include "../movgen/generate.hpp"
#include "../primitives/utility.hpp"

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
    
    ExtMove moves[MAX_MOVES];
    ExtMove *end = generate<LEGAL>(root_, moves);

    Move best_move = MOVE_NONE;
    int best_score = -VALUE_MATE, score;
    Board bb;
    for (auto it = moves; it != end; ++it) {
        Move m = it->move;
        bb = root_.do_move(m);

        stack_.push(root_.key(), m);
        score = -negamax(bb, limits_.max_depth);
        stack_.pop();

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

int SearchWorker::negamax(const Board &b, int depth) {
    check_time();
    if (!loop_.keep_going())
        return 0;

    if (b.half_moves() >= 100 || b.is_material_draw()
        || stack_.is_repetition(b.half_moves()))
        return 0;

    stats_.nodes++;
    if (depth <= 0)
        return eval(b);

    Board bb;
    ExtMove moves[MAX_MOVES];
    ExtMove *end = generate<LEGAL>(b, moves);
    int best_score = -VALUE_MATE, score, moves_tried = 0;
    for (auto it = moves; it != end; ++it, ++moves_tried) {
        Move m = it->move;

        bb = b.do_move(m);
        stack_.push(b.key(), m);
        score = -negamax(bb, depth - 1);
        stack_.pop();

        if (score > best_score)
            best_score = score;
    }

    if (!moves_tried) {
        if (b.checkers())
            return stack_.mated_score();
        return 0;
    }

    return best_score;
}

