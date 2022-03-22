#ifndef MOVEPICKER_HPP
#define MOVEPICKER_HPP

#include "movgen/generate.hpp"

enum class Stage {
    TT_MOVE = 0,

    INIT_TACTICAL,
    TACTICAL,
    /* GOOD_TACTICAL, */

    /* KILLER_1, */
    /* KILLER_2, */
    /* COUNTERMOVE, */

    /* BAD_TACTICAL, */

    INIT_NONTACTICAL,
    NON_TACTICAL,
};

class Board;

class MovePicker {
public:
    MovePicker(const Board &board, Move ttm = MOVE_NONE);

    template<bool qmoves>
    Move next();

    Stage stage() const;

private:
    void score_tactical();
    void score_nontactical();

    Move select();


    const Board &board_;
    ExtMove moves_[MAX_MOVES];
    ExtMove *cur_{}, *end_{};

    Move excluded_[1]{MOVE_NONE};
    Stage stage_;
};

#endif
