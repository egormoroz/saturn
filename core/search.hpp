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

struct SearchReport {
    SearchReport() = default;

    SearchReport(uint64_t nodes, uint64_t qnodes, uint64_t tt_hits, 
            float ordering, int16_t score, int time, 
            uint8_t depth, uint8_t sel_depth);

    Move pv[MAX_DEPTH];
    uint64_t nodes{}, qnodes{};
    uint64_t tt_hits{};
    float ordering{};

    int16_t score{};
    int time{};
    uint8_t depth{}, sel_depth{};
    uint8_t pv_len{};
};

std::ostream& operator<<(std::ostream &os, 
        const SearchReport &rep);

struct SearchListener {
    virtual void accept(int idx, const SearchReport &report) = 0;
    virtual void on_search_finished(int idx) = 0;

    virtual ~SearchListener() = default;
};

struct RootMove {
    Move m = MOVE_NONE;
    int score = -VALUE_INFINITE;
    int prev_score = -VALUE_INFINITE;

    bool operator==(const RootMove &y) const 
    { return m == y.m; }

    bool operator<(const RootMove &y) const { 
        if (score != y.score)
            return score > y.score;
        return prev_score > y.prev_score;
    }
};

class SearchContext {
public:
    explicit SearchContext(int id = 0);

    static void init_tables();

    void run(const Board &root, const SearchStack *ss,
            int max_depth, int max_millis, bool infinite);
    void stop_searching();
    void wait_for_completion();

    void add_listener(SearchListener *listener);

    ~SearchContext();

private:
    void reset();
    void iterative_deepening();
    int aspiration_window(int score, int depth);

    int search_root(const Board &b, int alpha, int beta,
            int depth);

    template<NodeType N>
    int search(const Board &b, int alpha, int beta,
            int depth, int ply, bool do_null = true);

    int quiesce(int alpha, int beta, const Board &b);

    template<NodeType N, bool reduce, bool defer>
    int search_move(const Board &before, const Board &after,
            Move played, int alpha, int beta, int depth, 
            int ply, int moves_tried, Move *deferred, 
            int &deferred_moves);

    bool stop();

    //shared_ptr?
    std::vector<SearchListener*> listeners_;
    int id_;

    Board root_;
    SearchStack ss_;
    std::array<Move, MAX_DEPTH> skip_move_;

    RootMove root_moves_[MAX_MOVES];
    int root_moves_nb_{};

    Killers killers_;
    CounterMoves counters_;
    HistoryHeuristic history_;

    Timer timer_;
    int max_depth_{};
    bool infinite_{};
    std::atomic_bool stop_{true};

    bool done_{true};
    std::atomic_bool quitting_{false};

    int sel_depth_{};
    uint64_t tt_hits_{};
    uint64_t nodes_{}, qnodes_{};
    int fh{}, fhf{};

    std::thread worker_;
    std::mutex mtx_;
    std::condition_variable cond_;

    static uint8_t LMR[32][64];
};

#endif
