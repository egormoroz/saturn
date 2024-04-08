#ifndef SEARCH_HPP
#define SEARCH_HPP

#include "../primitives/common.hpp"
#include "../parameters.hpp"

#include "../board/board.hpp"
#include "../searchstack.hpp"
#include "search_common.hpp"
#include "../movepicker.hpp"
#include "../evalcache.hpp"

#include <atomic>


struct RootMove {
    Move move;
    int16_t score;
};

class RootMovePicker {
public:
    RootMovePicker() = default;

    void reset(const Board &root);

    Move next();
    int num_moves() const;

    RootMove best_move() const;
    void complete_iter(int best_move_idx);

    // For multipv search. Excluded move == first pv move
    void mpv_reset();
    bool is_excluded(Move m) const;
    void exclude_top_move(int score);

    RootMove get_move(int idx) const;
    int num_excluded_moves() const;

private:
    std::array<RootMove, MAX_MOVES> moves_;
    int cur_{}, num_moves_{};

    int mpv_start_{};
};

class Search {
public:
    Search();

    void set_silent(bool s);

    void new_game();

    void setup(const Board &root, const SearchLimits &limits,
            const Stack *st = nullptr, bool ponder=false, int multipv = 1);

    void iterative_deepening();

    void atomic_stop();
    void stop_pondering();

    RootMove get_pv_start(int i) const;
    int num_pvs() const;

    const SearchStats& get_stats() const;

private:
    bool keep_going();
    int aspiration_window(int score, int depth);

    template<bool is_root = false>
    int search(const Board &b, int alpha, int beta, int depth);

    template<bool with_evasions>
    int quiescence(const Board &b, int alpha, int beta);

    bool is_board_drawn(const Board &b) const;

    int16_t evaluate(const Board &b);

    void extract_pvmoves();
    void uci_report() const;

    StateInfo root_si_;
    Board root_;
    Stack stack_;

    RootMovePicker rmp_;
    RootMove pv_moves_[MAX_MOVES];
    int n_pvs_;

    Histories hist_;
    std::array<Move, 64 * 64> counters_;
    std::array<Move, 64 * 64> followups_;

    TimeMan man_;
    SearchLimits limits_;
    SearchStats stats_;

    EvalCache ev_cache_;
    bool silent_ = false;
    std::atomic_bool keep_going_;
    std::atomic_bool pondering_ = false;
};

void update_reduction_tables(float k = params::defaults::lmr_coeff);

#endif
