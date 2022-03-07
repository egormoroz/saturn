#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "search.hpp"
#include <list>

class Engine : public SearchListener {
public:
    Engine(int threads = std::thread::hardware_concurrency());

    virtual void accept(int idx, const SearchReport &report) final;
    virtual void on_search_finished(int idx) final;

    //hist can be nullptr
    void start(const Board &b, const History *hist,
            int max_depth, int max_time);

    void stop_search();
    void wait_for_completion();

    //SearchContext desctructor cleans everything up for us
    ~Engine() = default;

private:
    std::list<SearchContext> searches_;

    std::mutex mtx_;
    SearchReport report_;
    int working_;
};

#endif
