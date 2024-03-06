#ifndef SEARCH_HPP
#define SEARCH_HPP

#include "../primitives/common.hpp"
#include "../searchstack.hpp"
#include "../board/board.hpp"
#include "search_common.hpp"
#include "../movepicker.hpp"
#include "../evalcache.hpp"
#include "../tt.hpp"
#include <atomic>

struct RootMove {
    Move move;
    int16_t score, prev_score;
    uint64_t nodes;
};

// In case of multipv we exclude the previous pv move for the next iteration
struct RMP {
    void reset(const Board &root);

    // The acutal first move from the first pv. Usually used for handling
    // situations with only a single legal move. 
    // It also usually is pulled from TT, so the score value may even be valid.
    const RootMove& first_move() const;

    Move next();
    void update_last(int score, uint64_t nodes);

    int num_moves() const;

    void complete_iter();
    void complete_iter_ttcutoff(Move ttm, int score);

    void new_id_iter();
    void complete_pvline();

    // Extract the pv, starting with the previous best move
    void extract_pv(const Board &root, PVLine &pv, int max_len);

private:
    std::array<RootMove, MAX_MOVES> moves_;

    // the index of the first non-pv move (the pv moves are at the top, unsorted)
    int start_{};
    int cur_{}, num_moves_{};
};

using MultiPV = std::array<PVLine, MAX_MOVES>;

class Search {
public:
    Search();

    void set_silent(bool s);

    void setup(const Board &root, const SearchLimits &limits,
            const Stack *st = nullptr);

    // Returns the number of pv lines.
    int iterative_deepening(int multipv);
    const MultiPV& get_pvs() const;

    void atomic_stop();

private:
    bool keep_going();

    int aspriration_window(int score, int depth);

    int search_root(int alpha, int beta, int depth);
    int search(const Board &b, int alpha, int beta, int depth);

    template<bool with_evasions>
    int quiescence(const Board &b, int alpha, int beta);

    bool is_draw() const;

    int16_t evaluate(const Board &b) const;

    void uci_report(int n_pvs) const;

    StateInfo root_si_;
    Board root_;
    Stack stack_;

    bool mpv_search_ = false;
    MultiPV pvs_;

    /* RootMovePicker rmp_; */
    RMP rmp_;
    Histories hist_;
    std::array<Move, 64 * 64> counters_;
    std::array<Move, 64 * 64> followups_;

    TimeMan man_;
    SearchLimits limits_;
    SearchStats stats_;

    EvalCache ev_cache_;
    bool silent_ = false;
    std::atomic_bool keep_going_;
};

void init_reduction_tables();

#endif
