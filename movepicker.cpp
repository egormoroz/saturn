#include "movepicker.hpp"
#include "board/board.hpp"
#include "movgen/generate.hpp"

void insertion_sort(ExtMove *begin, ExtMove *end) {
    for (ExtMove *p = begin + 1; p < end; ++p) {
        ExtMove x = *p, *q;
        for (q = p - 1; q >= begin && *q < x; --q)
            *(q + 1) = *q;
        *(q + 1) = x;
    }
}

MovePicker::MovePicker(const Board &b, Move ttm, int ply, 
        Move prev, const Killers &killers, const CounterMoves &counters,
        const HistoryHeuristic &history)
    : b_(b), ttm_(ttm), ply_(ply), prev_(prev), killers_(&killers), 
      counters_(&counters), history_(&history)
{
    stage_ = ttm != MOVE_NONE ? Stage::HASH 
        : INIT_CAPTURES;
    excluded_[0] = MOVE_NONE;
    excluded_[1] = MOVE_NONE;
    excluded_[2] = MOVE_NONE;
}

MovePicker::MovePicker(const Board &b)
    : b_(b), ttm_(MOVE_NONE), stage_(Stage::INIT_CAPTURES),
      ply_(0)  
{
}

constexpr int MVV_LVA[PIECE_TYPE_NB][PIECE_TYPE_NB] = {
    { 0,  0,  1,  1,  2,  3, 0 }, //noncapture promotions
    { 0,  7,  6,  6,  5,  4, 0 }, //?xPawn
    { 0, 11, 10, 10,  9,  8, 0 }, //?xKnight
    { 0, 11, 10, 10,  9,  8, 0 }, //?xBishop
    { 0, 15, 14, 14, 13, 12, 0 }, //?xRook
    { 0, 19, 18, 18, 17, 16, 0 }, //?xQueen
    { 0,  0,  0,  0,  0,  0, 0 }, //?xKing - not used
};

void MovePicker::score_captures() {
    ExtMove *ttm = nullptr;
    for (auto it = cur_; it != end_; ++it) {
        PieceType victim = type_of(b_.piece_on(to_sq(*it))),
                  attacker = type_of(b_.piece_on(from_sq(*it)));
        if (victim == NO_PIECE_TYPE)
            victim = prom_type(*it);
        if (*it == ttm_)
            ttm = it;
        it->value = MVV_LVA[victim][attacker];
    }

    if (cur_ != end_ && ttm)
        *ttm = *cur_++;
}

void MovePicker::score_quiets() {
    for (auto it = cur_; it != end_; ++it) {
        it->value = (*history_)
            [b_.side_to_move()][from_sq(*it)][to_sq(*it)];
    }
}

Move MovePicker::next() {
    Move m;

    switch (stage_) {
    case Stage::HASH:
        stage_ = Stage(stage_ + 1);
        return ttm_;
    case Stage::INIT_CAPTURES:
        stage_ = Stage(stage_ + 1);
        cur_ = moves_;
        end_ = generate<CAPTURES>(b_, moves_);
        score_captures();
        insertion_sort(cur_, end_);

        [[fallthrough]];

    case Stage::PICK_CAPTURES:
        if ((m = select([]() { return true; })) != MOVE_NONE) 
            return m;
        stage_ = Stage(stage_ + 1);

        [[fallthrough]];
    case Stage::PICK_KILLERS1:
        stage_ = Stage(stage_ + 1);
        m = (*killers_)[0][ply_];
        if (m != ttm_ && b_.is_valid_move(m)) {
            excluded_[0] = m;
            return m;
        }

        [[fallthrough]];
    case Stage::PICK_KILLERS2:
        stage_ = Stage(stage_ + 1);
        m = (*killers_)[1][ply_];
        if (m != ttm_ && b_.is_valid_move(m)) {
            excluded_[1] = m;
            return m;
        }

        [[fallthrough]];

    case Stage::PICK_COUNTERS:
        stage_ = Stage(stage_ + 1);
        m = (*counters_)[from_sq(prev_)][to_sq(prev_)];
        if (m != ttm_ && m != excluded_[0] && m != excluded_[1] 
                && b_.is_valid_move(m)) 
        {
            excluded_[2] = m;
            return m;
        }

        [[fallthrough]];

    case Stage::INIT_QUIETS:
        stage_ = Stage(stage_ + 1);
        cur_ = moves_;
        end_ = generate<QUIET>(b_, moves_);
        score_quiets();
        insertion_sort(cur_, end_);

        [[fallthrough]];
    case Stage::PICK_QUIETS:
        return select([this]() { 
            return *cur_ != ttm_ 
                && *cur_ != excluded_[0]
                && *cur_ != excluded_[1]
                && *cur_ != excluded_[2];
        });

    default:
        //unreachable
        return MOVE_NONE;
    };
}

Move MovePicker::qnext() {
    switch (stage_) {
    case Stage::INIT_CAPTURES:
        stage_ = Stage(stage_ + 1);
        cur_ = moves_;
        end_ = generate<CAPTURES>(b_, moves_);
        score_captures();
        insertion_sort(cur_, end_);

        [[fallthrough]];

    case Stage::PICK_CAPTURES:
        return select([]() { return true; });

    default:
        //unreachable
        return MOVE_NONE;
    }
}

template<typename Pred>
Move MovePicker::select(Pred filter) {
    for (; cur_ != end_; ++cur_) {
        if (filter())
            return *cur_++;
    }

    return MOVE_NONE;
}

