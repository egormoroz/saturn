#include "search.hpp"
#include "eval.hpp"
#include <type_traits>
#include <thread>
#include <iostream>
#include <sstream>
#include "../tt.hpp"
#include "../primitives/utility.hpp"
#include "../movgen/generate.hpp"

SearchContext::SearchContext() {
    reset();

    worker_ = std::thread([this]() {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx_);
            cond_.wait(lock, [this]() { return quitting_ || !done_; });
            if (quitting_)
                break;
            iterative_deepening();
            done_ = true;
        }
    });
}

void SearchContext::run(int max_depth, int max_millis, bool infinite) {
    stop_.store(true, std::memory_order_seq_cst);
    {
        std::lock_guard<std::mutex> lock(mtx_);
        reset();
        timer_.restart(Clock::now(), max_millis);
        infinite_ = infinite;
        max_depth_ = max_depth;

        stop_ = false;
        quitting_ = false;
        done_ = false;
    }
    cond_.notify_one();
}

void SearchContext::iterative_deepening() {
    Board b = root_; //make a copy

    Move pv[MAX_PLIES];

    int score = 0;
    std::ostringstream os;
    for (int depth = 1; depth <= max_depth_; ++depth) {
        score = search(-VALUE_INFINITE, VALUE_INFINITE, depth, b);
        if (stop())
            break;

        os.str("");
        int elapsed = timer_.elapsed_millis();
        os << "info score cp " << score << " depth " << depth 
           << " nodes " << nodes_ << " time " << elapsed << " pv ";

        int n = g_tt.extract_pv(b, pv);
        for (int i = 0; i < n; ++i)
            os << pv[i] << ' ';
        sync_cout << os.str() << sync_endl;
    }

    TTEntry tte;
    if (g_tt.probe(b.key(), tte) == HASH_HIT)
        sync_cout << "bestmove " << Move(tte.move16) << sync_endl;
}

void SearchContext::set_board(const Board &b) {
    stop_ = true;
    std::lock_guard<std::mutex> lock(mtx_);
    root_ = b;
}


void SearchContext::accept(const UCI::Command &cmd) {
    //This function gets called from the UCI thread!!
    using namespace UCI::cmd;

    std::visit([this](auto &&v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, Position>) {
            set_board(v.board);
        } else if constexpr (std::is_same_v<T, Go>) {

            reset();
            Color us = root_.side_to_move();
            int millis_left = v.time_left[us],
                inc = v.increment[us];
            int max_millis = millis_left / 55 + inc - 200;

            run(v.max_depth, max_millis, v.infinite);
        } else if constexpr (std::is_same_v<T, Stop>) {
            stop_.store(true, std::memory_order_relaxed);
        } else if constexpr (std::is_same_v<T, Quit>) {
            stop_.store(true, std::memory_order_relaxed);
            quitting_.store(true, std::memory_order_relaxed);
        }
    }, cmd);
}

void SearchContext::wait_for_search() {
    while (true) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (done_)
            break;
    }
}

void SearchContext::reset() {
    infinite_ = false;
    max_depth_ = 0;
    nodes_ = 0;
}

int SearchContext::search(int alpha, int beta, 
        int depth, const Board &b) 
{
    ++nodes_;
    if (depth == 0)
        return eval(b);
    if (stop())
        return 0;

    Bound bound = BOUND_ALPHA;
    Move best_move = MOVE_NONE;

    ExtMove begin[MAX_MOVES], *end;
    end = generate<LEGAL>(b, begin);

    for (auto *it = begin; it != end; ++it) {
        Move m = *it;
        int score = -search(-beta, -alpha, depth - 1, 
                b.do_move(m));

        if (score > alpha) {
            if (score >= beta) {
                g_tt.store(TTEntry(b.key(), beta, BOUND_BETA, 
                            depth, best_move, false));
                return beta;
            }

            alpha = score;
            best_move = m;
            bound = BOUND_EXACT;
        }
    }

    g_tt.store(TTEntry(b.key(), alpha, bound, depth, 
                best_move, false));
    return alpha;
}

bool SearchContext::stop() {
    if (nodes_ & 8191) {
        if (stop_ || (!infinite_ && timer_.out_of_time()))
            return true;
    }
    return false;
}

SearchContext::~SearchContext() {
    stop_ = true;
    quitting_ = true;
    cond_.notify_one();
    if (worker_.joinable())
        worker_.join();
}

