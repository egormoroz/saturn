#ifndef SEARCHWORKER_HPP
#define SEARCHWORKER_HPP

#include "../primitives/common.hpp"
#include "../searchstack.hpp"
#include "../board/board.hpp"
#include "search_common.hpp"
#include "routine.hpp"
#include "../movepicker.hpp"

struct RootMove {
    Move move;
    int16_t score, prev_score;
    uint64_t nodes;
};

class RootMovePicker {
public:
    RootMovePicker() = default;

    void reset(const Board &root);

    Move first() const;
    Move next();
    void update_last(int score, uint64_t nodes);

    int num_moves() const;

    void complete_iter();

private:
    std::array<RootMove, MAX_MOVES> moves_;
    int cur_{}, num_moves_{};
};

class SearchWorker {
public:
    SearchWorker();

    void go(const Board &root, const Stack &st,
            const SearchLimits &limits);

    void stop();
    void wait_for_completion();

private:
    void check_time();
    void iterative_deepening();
    int aspriration_window(int score, int depth);

    int search_root(int alpha, int beta, int depth);
    int search(const Board &b, int alpha, int beta, int depth);

    template<bool with_evasions>
    int quiescence(const Board &b, int alpha, int beta);

    Board root_;
    Stack stack_;

    RootMovePicker rmp_;
    Histories hist_;
    std::array<Move, 64 * 64> counters_;
    std::array<Move, 64 * 64> followups_;

    TimeMan man_;
    SearchLimits limits_;
    SearchStats stats_;

    Routine loop_;
};

void init_reduction_tables();

#endif
