#include "engine.hpp"
#include <iostream>
#include "../primitives/utility.hpp"

Engine::Engine(int threads) {
    for (int i = 0; i < threads; ++i) {
        searches_.emplace_back(i);
        searches_.back().add_listener(this);
    }
}

void Engine::accept(int idx, const SearchReport &report) {
    sync_cout << "thread " << idx << ": " << report << sync_endl;
    nodes_ += report.nodes();
}

void Engine::on_search_finished(int idx, Move best_move) {
    sync_cout << "thread " << idx << ": " 
        << best_move << sync_endl;
}

void Engine::start(const Board &b, int max_depth, int max_time) {
    nodes_.store(0, std::memory_order_relaxed);
    for (auto &s: searches_) {
        s.wait_for_completion();
        s.set_board(b);
    }

    for (auto &s: searches_)
        s.run(max_depth, max_time, max_time < 0);
}

void Engine::abort_search() {
    for (auto &s: searches_)
        s.abort_search();
}

void Engine::wait_for_completion() {
    for (auto &s: searches_)
        s.wait_for_completion();
}

int Engine::total_nodes() const {
    return nodes_.load(std::memory_order_relaxed);
}

