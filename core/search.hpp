#ifndef SEARCH_HPP
#define SEARCH_HPP

#include "../timer.hpp"
#include "../uci.hpp"
#include <mutex>
#include <condition_variable>
#include <thread>

class SearchContext : public UCI::Listener {
public:
    SearchContext();

    void run(int max_depth, int max_millis, bool infinite);

    void set_board(const Board &b);
    virtual void accept(const UCI::Command &cmd) override;

    void wait_for_search();

    ~SearchContext();

private:
    void reset();
    int search(int alpha, int beta, int depth, const Board &b);
    void iterative_deepening();

    bool stop();

    Board root_;

    Timer timer_;
    int max_depth_{};
    bool infinite_{};
    std::atomic_bool stop_{true};

    bool done_{true};
    std::atomic_bool quitting_{false};

    int nodes_{};

    std::thread worker_;
    std::mutex mtx_;
    std::condition_variable cond_;
};

#endif
