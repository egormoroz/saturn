#ifndef SEARCHWORKER_HPP
#define SEARCHWORKER_HPP

#include "../primitives/common.hpp"
#include "../searchstack.hpp"
#include "../board/board.hpp"
#include "search_common.hpp"
#include "routine.hpp"

struct RootMove {
    Move move;
    int16_t score;
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

    int search(const Board &b, int alpha, int beta, int depth);

    template<bool with_evasions>
    int quiescence(const Board &b, int alpha, int beta);

    Board root_;
    Stack stack_;

    TimeMan man_;
    SearchLimits limits_;
    SearchStats stats_;

    Routine loop_;
};

#endif
