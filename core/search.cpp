#include "search.hpp"
#include "eval.hpp"
#include <type_traits>
#include <thread>
#include <iostream>
#include <sstream>
#include <algorithm>
#include "../tt.hpp"
#include "../primitives/utility.hpp"
#include <cstring>


SearchReport::SearchReport(uint32_t nodes, uint32_t tt_hits, 
        float ordering, int16_t score, 
        uint16_t time, uint8_t depth)
    : nodes_(nodes), tt_hits_(tt_hits), ordering_(ordering),
      score_(score), time_(time), depth_(depth), pv_len_(0)
{
}

Move SearchReport::pv_move(int idx) const {
    return idx >= pv_len_ ? MOVE_NONE : pv_[idx];
}

int SearchReport::pv_len() const { return pv_len_; }
Move SearchReport::best_move() const { return pv_move(0); }

uint32_t SearchReport::nodes() const { return nodes_; }
uint32_t SearchReport::tt_hits() const { return tt_hits_; }
float SearchReport::ordering() const { return ordering_; }

int SearchReport::score() const { return score_; }
int SearchReport::time() const { return time_; }
int SearchReport::depth() const { return depth_; }

std::ostream& operator<<(std::ostream &os, 
        const SearchReport &rep) 
{
    os << "info score " << Score{rep.score()} << " depth " 
       << rep.depth() << " nodes " << rep.nodes() 
       << " time " << rep.time() << " tt_hits " << rep.tt_hits()
       << " ordering: " << rep.ordering()
       << " pv ";

    for (int i = 0; i < rep.pv_len(); ++i)
        os << rep.pv_move(i) << ' ';

    return os;
}

SearchContext::SearchContext(int id) : id_(id) {
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
    abort_search();
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

void SearchContext::abort_search() {
    stop_ = true;
}

void SearchContext::iterative_deepening() {
    Move pv[MAX_PLIES];
    int score = 0, alpha = -VALUE_INFINITE, beta = VALUE_INFINITE;

    auto report = [&](int depth) {
        SearchReport rep(nodes_, tt_hits_, fhf / float(fh),
                score, timer_.elapsed_millis(), depth);
        rep.pv_len_ = g_tt.extract_pv(root_, pv, depth);
        for (int i = 0; i < rep.pv_len_; ++i)
            rep.pv_[i] = pv[i];

        for (auto &l: listeners_)
            l->accept(id_, rep);
    };

    score = search_root(root_, alpha, beta, 1);
    report(1);

    constexpr int WINDOW = PAWN_VALUE / 4;
    for (int depth = 2; depth <= max_depth_; ++depth) {
        alpha = score - WINDOW; beta = score + WINDOW;
        score = search_root(root_, alpha, beta, depth);
        if (score <= alpha || score >= beta)
            score = search_root(root_, -VALUE_INFINITE, 
                    VALUE_INFINITE, depth);

        if (stop()) break;

        report(depth);

        if (abs(score) > VALUE_MATE - 100)
            break;
    }

    for (auto &l: listeners_)
        l->on_search_finished(id_, pv[0]);
}

void SearchContext::set_board(const Board &b) {
    abort_search();
    std::lock_guard<std::mutex> lock(mtx_);
    root_ = b;
    hist_.ply = 0;
}


void SearchContext::accept(const UCI::Command &cmd) {
    //This function gets called from the UCI thread!!
    using namespace UCI::cmd;

    std::visit([this](auto &&v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, Position>) {
            abort_search();
            std::lock_guard<std::mutex> lock(mtx_);
            root_ = v.board;
            hist_ = v.hist;
        } else if constexpr (std::is_same_v<T, Go>) {
            Color us = root_.side_to_move();
            int millis_left = v.time_left[us],
                inc = v.increment[us];
            int max_millis = millis_left / 55 + inc - 200;
            max_millis = std::max(max_millis, v.move_time);

            run(v.max_depth, max_millis, v.infinite);
        } else if constexpr (std::is_same_v<T, Stop>) {
            abort_search();
        } else if constexpr (std::is_same_v<T, Quit>) {
            quitting_.store(true, std::memory_order_relaxed);
            abort_search();
        }
    }, cmd);
}

void SearchContext::wait_for_completion() {
    while (true) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (done_)
            break;
    }
}

void SearchContext::add_listener(SearchListener *listener) {
    listeners_.push_back(listener);
}

void SearchContext::reset() {
    infinite_ = false;
    max_depth_ = 0;
    nodes_ = 0;
    tt_hits_ = 0;
    fhf = 0;
    fh = 0;

    memset(killers_.data(), 0, sizeof(killers_));
    memset(counters_.data(), 0, sizeof(counters_));
    memset(history_.data(), 0, sizeof(history_));
}

bool SearchContext::stop() {
    if (nodes_ & 8191)
        if (stop_ || (!infinite_ && timer_.out_of_time()))
            stop_ = true;
    return stop_;
}

SearchContext::~SearchContext() {
    abort_search();
    quitting_ = true;
    cond_.notify_one();
    if (worker_.joinable())
        worker_.join();
}

