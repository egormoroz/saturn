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


SearchReport::SearchReport(uint64_t nodes, uint64_t tt_hits, 
        float ordering, int16_t score, 
        int time, uint8_t depth)
    : nodes(nodes), tt_hits(tt_hits), ordering(ordering),
      score(score), time(time), depth(depth), pv_len(0)
{
}

std::ostream& operator<<(std::ostream &os, 
        const SearchReport &rep) 
{
    uint64_t nps = rep.nodes / (rep.time / 1000 + 1);
    os << "info score " << Score{rep.score} << " depth " 
       << int(rep.depth) << " nodes " << rep.nodes 
       << " time " << rep.time << " nps " << nps
       << " tt_hits " << rep.tt_hits
       << " ordering: " << rep.ordering
       << " pv ";

    for (int i = 0; i < rep.pv_len; ++i)
        os << rep.pv[i] << ' ';

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

void SearchContext::run(const Board &b, const History *hist,
        int max_depth, int max_millis, bool infinite) 
{
    stop_searching();
    {
        std::lock_guard<std::mutex> lock(mtx_);
        reset();

        root_ = b;
        hist_.ply = 0;
        if (hist)
            hist_ = *hist;

        timer_.restart(Clock::now(), max_millis);
        infinite_ = infinite;
        max_depth_ = max_depth;

        stop_ = false;
        quitting_ = false;
        done_ = false;
    }
    cond_.notify_one();
}

void SearchContext::stop_searching() {
    stop_ = true;
}

void SearchContext::iterative_deepening() {
    Move pv[MAX_PLIES];
    int score = 0, alpha = -VALUE_INFINITE, beta = VALUE_INFINITE;

    auto report = [&](int depth) {
        SearchReport rep(nodes_, tt_hits_, fhf / float(fh),
                score, timer_.elapsed_millis(), depth);
        rep.pv_len = g_tt.extract_pv(root_, pv, MAX_PLIES);
        for (int i = 0; i < rep.pv_len; ++i)
            rep.pv[i] = pv[i];

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
        l->on_search_finished(id_);
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
    if ((nodes_ & 1023) == 0)
        if (stop_ || (!infinite_ && timer_.out_of_time()))
            stop_ = true;
    return stop_;
}

SearchContext::~SearchContext() {
    stop_searching();
    quitting_ = true;
    cond_.notify_one();
    if (worker_.joinable())
        worker_.join();
}

