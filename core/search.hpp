#ifndef SEARCH_HPP
#define SEARCH_HPP

#include "../timer.hpp"
#include "../uci.hpp"
#include "../movepicker.hpp"
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>

enum NodeType {
    IS_PV,
    NO_PV,
};

class SearchReport {
    friend class SearchContext;
public:
    SearchReport(uint32_t nodes, uint32_t tt_hits, 
            float ordering, int16_t score, 
            uint16_t time, uint8_t depth);

    Move pv_move(int idx) const;
    int pv_len() const;
    Move best_move() const;

    uint32_t nodes() const;
    uint32_t tt_hits() const;
    float ordering() const;

    int score() const;
    int time() const;
    int depth() const;

private:
    Move pv_[MAX_PLIES];
    uint32_t nodes_;
    uint32_t tt_hits_;
    float ordering_;

    int16_t score_;
    uint16_t time_;
    uint8_t depth_;
    uint8_t pv_len_;
};

std::ostream& operator<<(std::ostream &os, 
        const SearchReport &rep);

struct SearchListener {
    virtual void accept(int idx, const SearchReport &report) = 0;
    virtual void on_search_finished(int idx, Move best_move) = 0;

    virtual ~SearchListener() = default;
};

/*
 * TODO: get rid of UCI Listener
 * */
class SearchContext : public UCI::Listener {
public:
    explicit SearchContext(int id = 0);

    void run(int max_depth, int max_millis, bool infinite);
    void abort_search();

    void set_board(const Board &b);
    virtual void accept(const UCI::Command &cmd) override;

    void wait_for_completion();

    void add_listener(SearchListener *listener);

    ~SearchContext();

private:
    void reset();
    void iterative_deepening();

    int search_root(const Board &b, int alpha, int beta,
            int depth);

    template<NodeType N>
    int search(const Board &b, int alpha, int beta,
            int depth, int ply, bool do_null = true);

    int quiesce(int alpha, int beta, const Board &b);

    bool stop();

    //shared_ptr?
    std::vector<SearchListener*> listeners_;
    int id_;

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
