#ifndef MOVEPICKER_HPP
#define MOVEPICKER_HPP

#include "movgen/generate.hpp"

enum class Stage {
    TT_MOVE = 0,

    INIT_TACTICAL,
    GOOD_TACTICAL,

    KILLER_1,
    KILLER_2,

    COUNTER_MOVE,
    FOLLOW_UP,

    BAD_TACTICAL,

    INIT_NONTACTICAL,
    NON_TACTICAL,
};

class Board;

struct Histories {
    std::array<
        std::array<
            std::array<int16_t, SQUARE_NB>, 
            SQUARE_NB>,
        COLOR_NB> main;

    void reset();
    void add_bonus(const Board &b, Move m, int16_t bonus);
    void update(const Board &b, Move bm, int depth,
            const Move *quiets, int nq);

    int16_t get_score(const Board &b, Move m) const;
};

class MovePicker {
public:
    MovePicker(const Board &board, Move ttm,
        const Move *killers = nullptr,
        const Histories *histories = nullptr,
        Move counter = MOVE_NONE,
        Move followup = MOVE_NONE);
    //for quiescence
    MovePicker(const Board &board);

    template<bool qmoves>
    Move next();

    Stage stage() const;

private:
    void score_tactical();
    void score_nontactical();

    struct AnyMove {
        bool operator()() const { return true; }
    };

    template<typename F = AnyMove>
    Move select(F &&filter = AnyMove());

    const Board &board_;
    ExtMove moves_[MAX_MOVES];
    ExtMove *cur_{}, *end_bad_caps_{}, *end_{};

    Move ttm_{}, counter_{}, followup_{};
    std::array<Move, 2> killers_;
    const Histories *hist_{};
    Stage stage_;
};

#endif
