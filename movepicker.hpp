#ifndef MOVEPICKER_HPP
#define MOVEPICKER_HPP

#include "primitives/common.hpp"
#include "movgen/generate.hpp"
#include <array>

class Board;

enum Stage {
    HASH,
    INIT_CAPTURES,
    GOOD_CAPTURES,
    PICK_KILLERS1,
    PICK_KILLERS2,
    PICK_COUNTERS,
    BAD_CAPTURES,
    INIT_QUIETS,
    PICK_QUIETS
};

template<size_t N, size_t M>
using Stats = std::array<std::array<Move, M>, N>;

//using Killers = std::array<std::array<Move, MAX_DEPTH>, 2>;
using Killers = Stats<2, MAX_DEPTH>;
using CounterMoves = Stats<SQUARE_NB, SQUARE_NB>;
using HistoryHeuristic = std::array<std::array<
        std::array<uint16_t, SQUARE_NB>, SQUARE_NB>, COLOR_NB>;

class MovePicker {
public:
    //usual search
    MovePicker(const Board &b, Move ttm, int ply, Move prev,
            const Killers &killers, const CounterMoves &counters,
            const HistoryHeuristic &history);
    //quiescence search
    MovePicker(const Board &b);

    Move next();
    Move qnext();

    Stage stage() const;

private:
    void score_captures();
    void score_quiets();

    template<typename Pred>
    Move select(Pred filter);

    const Board &b_;
    Move ttm_;
    Stage stage_;

    int ply_;
    Move prev_{MOVE_NONE};
    const Killers *killers_{};
    const CounterMoves *counters_{};
    const HistoryHeuristic *history_{};

    ExtMove moves_[MAX_MOVES];
    ExtMove *cur_{}, *end_{}, *end_bad_caps_{};

    Move excluded_[3];
};

#endif
