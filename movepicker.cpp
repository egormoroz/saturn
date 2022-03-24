#include "movepicker.hpp"
#include "board/board.hpp"
#include <algorithm>

namespace {

void insertion_sort(ExtMove *begin, ExtMove *end) {
    for (ExtMove *p = begin + 1; p < end; ++p) {
        ExtMove x = *p, *q;
        for (q = p - 1; q >= begin && *q < x; --q)
            *(q + 1) = *q;
        *(q + 1) = x;
    }
}

constexpr int16_t MVV_LVA[PIECE_TYPE_NB][PIECE_TYPE_NB] = {
    { 0,  0,  1,  1,  2,  3, 0 }, //noncapture promotions
    { 0,  7,  6,  6,  5,  4, 0 }, //?xPawn
    { 0, 11, 10, 10,  9,  8, 0 }, //?xKnight
    { 0, 11, 10, 10,  9,  8, 0 }, //?xBishop
    { 0, 15, 14, 14, 13, 12, 0 }, //?xRook
    { 0, 19, 18, 18, 17, 16, 0 }, //?xQueen
    { 0,  0,  0,  0,  0,  0, 0 }, //?xKing - not used
};

//http://www.talkchess.com/forum3/viewtopic.php?t=66312
//This one is huuge
constexpr int16_t SortingTypes[PIECE_TYPE_NB] = {0, 10, 8, 8, 4, 3, 1};

constexpr int16_t SortingTable[SQUARE_NB] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 2, 2, 2, 2, 2, 2, 1,
    1, 2, 4, 4, 4, 4, 2, 1,
    1, 2, 4, 6, 6, 4, 2, 1,
    1, 2, 4, 6, 6, 4, 2, 1,
    1, 2, 4, 4, 4, 4, 2, 1,
    1, 2, 2, 2, 2, 2, 2, 1,
    0, 0, 0, 0, 0, 0, 0, 0,
};

}

void Histories::reset() {
    memset(main.data(), 0, sizeof(main));
}

void Histories::add_bonus(const Board &b, Move m, int16_t bonus) {
    Square from = from_sq(m), to = to_sq(m);
    Piece p = b.piece_on(from);

    int16_t &entry = main[p][to];
    entry += 32 * bonus - entry * abs(bonus) / 512;
}

MovePicker::MovePicker(const Board &board, Move ttm,
        const Move *killers, const Histories *histories,
        Move counter, Move followup)
    : board_(board), ttm_(ttm), counter_(counter), 
      followup_(followup), hist_(histories),
      stage_(ttm ? Stage::TT_MOVE : Stage::INIT_TACTICAL)
{
    if (killers) {
        killers_[0] = killers[0];
        killers_[1] = killers[1];
    }
}

MovePicker::MovePicker(const Board &board)
    : board_(board), stage_(Stage::INIT_TACTICAL)
{}

template Move MovePicker::next<true>();
template Move MovePicker::next<false>();

template<bool qmoves>
Move MovePicker::next() {
    Move m;
    switch (stage_) {
    case Stage::TT_MOVE:
        stage_ = Stage::INIT_TACTICAL;
        return ttm_;
    case Stage::INIT_TACTICAL:
        stage_ = Stage::GOOD_TACTICAL;
        end_bad_caps_ = cur_ = moves_;
        end_ = generate<TACTICAL>(board_, cur_);
        score_tactical();
        insertion_sort(cur_, end_);

        [[fallthrough]];
    case Stage::GOOD_TACTICAL:
        m = select([this]() {
            if (board_.see_ge(*cur_)) return true;
            *end_bad_caps_++ = *cur_;
            return false;
        });
        if (m != MOVE_NONE) return m;

        if constexpr (qmoves)
            return MOVE_NONE;
        stage_ = Stage::KILLER_1;

        [[fallthrough]];
    case Stage::KILLER_1:
        stage_ = Stage::KILLER_2;
        if (killers_[0] != ttm_ && 
                board_.is_valid_move(killers_[0]))
            return killers_[0];
        [[fallthrough]];

    case Stage::KILLER_2:
        stage_ = Stage::COUNTER_MOVE;
        if (killers_[1] != ttm_ && 
                board_.is_valid_move(killers_[1]))
            return killers_[1];
        [[fallthrough]];

    case Stage::COUNTER_MOVE:
        stage_ = Stage::FOLLOW_UP;
        if (counter_ != ttm_ 
            && counter_ != killers_[0] 
            && counter_ != killers_[1]
            && board_.is_valid_move(counter_))
            return counter_;
        [[fallthrough]];

    case Stage::FOLLOW_UP:
        stage_ = Stage::BAD_TACTICAL;
        cur_ = moves_;
        end_ = end_bad_caps_;
        if (followup_ != ttm_
            && followup_ != killers_[0]
            && followup_ != killers_[1]
            && followup_ != counter_
            && board_.is_valid_move(followup_))
            return followup_;
        [[fallthrough]];

    case Stage::BAD_TACTICAL:
        if ((m = select()) != MOVE_NONE)
            return m;

        stage_ = Stage::INIT_NONTACTICAL;
        [[fallthrough]];
    case Stage::INIT_NONTACTICAL:
        stage_ = Stage::NON_TACTICAL;
        cur_ = moves_;
        end_ = generate<NON_TACTICAL>(board_, cur_);
        score_nontactical();
        insertion_sort(cur_, end_);

        [[fallthrough]];
    case Stage::NON_TACTICAL:
        return select([this]() {
            return *cur_ != killers_[0]
                && *cur_ != killers_[1]
                && *cur_ != counter_
                && *cur_ != followup_;
        });
    };

    //unreachable
    return MOVE_NONE;
}


Stage MovePicker::stage() const { return stage_; }

void MovePicker::score_tactical() {
    for (auto it = cur_; it != end_; ++it) {
        PieceType victim = type_of(board_.piece_on(to_sq(*it))),
                  attacker = type_of(board_.piece_on(from_sq(*it)));
        if (victim == NO_PIECE_TYPE)
            victim = prom_type(*it);

        it->value = MVV_LVA[victim][attacker];
    }
}

void MovePicker::score_nontactical() {
    for (auto it = cur_; it != end_; ++it) {
        Square from = from_sq(*it), to = to_sq(*it);
        Piece p = board_.piece_on(from);
        int16_t k = SortingTypes[type_of(p)];
        it->value = k * (SortingTable[to] - SortingTable[from]);
        if (hist_)
            it->value += hist_->main[p][to];
    }
}

template<typename F>
Move MovePicker::select(F &&filter) {
    for (; cur_ != end_; ++cur_) {
        if (*cur_ != ttm_ && filter())
            return *cur_++;
    }

    return MOVE_NONE;
}

