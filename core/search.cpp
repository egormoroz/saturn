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
#include <cmath>

uint8_t SearchContext::LMR[32][64];


SearchReport::SearchReport(uint64_t nodes, uint64_t qnodes, uint64_t tt_hits, 
        float ordering, int16_t score, 
        int time, uint8_t depth)
    : nodes(nodes), qnodes(qnodes), tt_hits(tt_hits), ordering(ordering),
      score(score), time(time), depth(depth), pv_len(0)
{
}

std::ostream& operator<<(std::ostream &os, 
        const SearchReport &rep) 
{
    uint64_t nps = 1000 * rep.nodes / (rep.time + 1);
    float qratio = rep.qnodes / float(rep.nodes);
    os << "info score " << Score{rep.score} << " depth " 
       << int(rep.depth) << " nodes " << rep.nodes 
       << " qnodes " << rep.qnodes << " qratio " << qratio
       << " time " << rep.time << " nps " << nps
       << " tt_hits " << rep.tt_hits
       << " ordering " << rep.ordering
       << " hashfull " << g_tt.hashfull()
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

void SearchContext::run(const Board &b, const SearchStack *ss,
        int max_depth, int max_millis, bool infinite) 
{
    stop_searching();
    {
        std::lock_guard<std::mutex> lock(mtx_);
        reset();

        root_ = b;
        ss_.ply = 0;
        if (ss)
            ss_ = *ss;

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
    int score = 0, alpha = -VALUE_INFINITE, beta = VALUE_INFINITE;

    auto report = [&](int depth) {
        SearchReport rep(nodes_, qnodes_, tt_hits_, fhf / float(fh),
                score, timer_.elapsed_millis(), depth);
        rep.pv_len = g_tt.extract_pv(root_, rep.pv, depth);
        if (!rep.pv_len) {
            sync_cout << "info string WARNING! NO PV MOVE" << sync_endl;
            rep.pv[0] = root_moves_nb_ ? root_moves_[0].m
                : MOVE_NONE;
            rep.pv_len = 1;
        }

        for (auto &l: listeners_)
            l->accept(id_, rep);
    };

    root_moves_nb_ = 0;
    score = search_root(root_, alpha, beta, 1);
    report(1);

    for (int depth = 2; depth <= max_depth_; ++depth) {
        score = depth >= 4 ? aspiration_window(score, depth)
            : search_root(root_, -VALUE_INFINITE, VALUE_INFINITE, depth);

        if (stop()) break;

        report(depth);

        if (abs(score) >= VALUE_MATE - depth)
            break;
    }

    for (auto &l: listeners_)
        l->on_search_finished(id_);
}

int SearchContext::aspiration_window(int score, int depth) {
    int delta = 16;
    int alpha = score - delta, beta = score + delta;

    while (!stop()) {
        score = search_root(root_, alpha, beta, depth);

        if (score <= alpha) {
            beta = (alpha + beta) / 2;
            alpha = std::max(-VALUE_INFINITE, alpha - delta);
        } else if (score >= beta) {
            beta = std::min(int(VALUE_INFINITE), beta + delta);
        } else {
            return score;
        }

        delta = delta + delta / 2;
    }

    return 0;
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
    qnodes_ = 0;
    tt_hits_ = 0;
    fhf = 0;
    fh = 0;
    root_moves_nb_ = 0;

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

void SearchContext::init_tables() {
    for (int d = 1; d < 32; ++d)
        for (int m = 1; m < 64; ++m)
            LMR[d][m] = 0.1 + log(d) * log(m) / 2;

    LMR[0][0] = LMR[0][1] = LMR[1][0] = 0;
}

