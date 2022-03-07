#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "search.hpp"
#include <list>

class Engine : public SearchListener {
public:
    Engine(int threads = std::thread::hardware_concurrency());

    virtual void accept(int idx, const SearchReport &report) final;
    virtual void on_search_finished(int idx, Move best_move) final;

    void start(const Board &b, int max_depth, int max_time);

    void abort_search();
    void wait_for_completion();

    int total_nodes() const;

    //SearchContext desctructor cleans everything up for us
    ~Engine() = default;

private:
    std::list<SearchContext> searches_;
    std::atomic_int nodes_{};
};

#endif
