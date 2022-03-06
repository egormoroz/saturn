#ifndef SEARCH_HPP
#define SEARCH_HPP

#include "../timer.hpp"
#include "../uci.hpp"
#include "../movepicker.hpp"
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
    void iterative_deepening();

    int search(const Board &b, int alpha, int beta, 
            int depth, int ply = 0);
    int quiesce(int alpha, int beta, const Board &b);

    //does the move and some bookkeeping
    Board do_move(const Board &b, Move m);
    //again, for bookkeeping purposes
    void undo_move();

    bool stop();

    Board root_;
    History hist_;

    Killers killers_;
    CounterMoves counters_;
    HistoryHeuristic history_;

    Timer timer_;
    int max_depth_{};
    bool infinite_{};
    std::atomic_bool stop_{true};

    bool done_{true};
    std::atomic_bool quitting_{false};

    int tt_hits_{};
    int nodes_{};
    int fh{}, fhf{};

    std::thread worker_;
    std::mutex mtx_;
    std::condition_variable cond_;
};

#endif
