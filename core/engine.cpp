#include "engine.hpp"
#include <iostream>
#include "../primitives/utility.hpp"
#include "../uci.hpp"
#include "../tt.hpp"

Engine::Engine(int threads) {
    for (int i = 0; i < threads; ++i) {
        searches_.emplace_back(i);
        searches_.back().add_listener(this);
    }
}

void Engine::accept(int, const SearchReport &new_rep) {
    std::lock_guard<std::mutex> lock(mtx_);
    uint64_t nodes = report_.nodes + new_rep.nodes;
    if (new_rep.depth > report_.depth && new_rep.pv_len) {
        report_ = new_rep;
        report_.nodes = nodes;
        sync_cout << report_ << sync_endl;
    }
    report_.nodes = nodes;
}

void Engine::on_search_finished(int) {
    std::lock_guard<std::mutex> lock(mtx_);
    --working_;
    if (!working_) {
        assert(report_.pv_len);
        sync_cout << "bestmove " << report_.pv[0] << sync_endl;
    }
}

void Engine::start(const Board &b, const SearchStack *ss,
        int max_depth, int max_time) 
{
    for (auto &s: searches_)
        s.wait_for_completion();

    g_tt.new_search();
    working_ = searches_.size();
    report_ = SearchReport();
    for (auto &s: searches_)
        s.run(b, ss, max_depth, max_time, max_time < 0);
}

void Engine::stop_search() {
    for (auto &s: searches_)
        s.stop_searching();
}

void Engine::wait_for_completion() {
    for (auto &s: searches_)
        s.wait_for_completion();
}

