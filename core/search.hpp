#ifndef SEARCH_HPP
#define SEARCH_HPP

#include "../primitives/common.hpp"
#include "../searchstack.hpp"
#include "../board/board.hpp"
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

struct UCISearchConfig {
    int multipv = defopts::MULTIPV;
    int move_overhead = defopts::MOVE_OVERHEAD;

    int asp_init_delta = defopts::ASP_INIT_DELTA;
    int asp_min_depth = defopts::ASP_MIN_DEPTH;
};

class Search {
public:
    Search();

    void set_silent(bool s);

    void setup(const Board &root, const SearchLimits &limits,
            UCISearchConfig usc, const Stack *st = nullptr, bool ponder=false);

    void iterative_deepening();

    void atomic_stop();
    void stop_pondering();

    RootMove get_pv_start(int i) const;
    int num_pvs() const;

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
    UCISearchConfig uci_cfg_;

    EvalCache ev_cache_;
    bool silent_ = false;
    std::atomic_bool keep_going_;
    std::atomic_bool pondering_ = false;
};

void init_reduction_tables(float k = defopts::LMR_COEFF);

#endif
