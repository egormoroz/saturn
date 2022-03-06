#include "board.hpp"
#include <cassert>
#include "../movgen/attack.hpp"

bool Board::ok_capture(Move m) const {
    assert(is_ok(m));
    assert(type_of(m) == NORMAL);

    auto value_on = [this](Square s) { 
        return PIECE_VALUE[type_of(piece_on(s))];
    };

    Square from = from_sq(m), to = to_sq(m);

    int balance = value_on(from) - value_on(to);
    if (balance <= 0)
        return true;

    Bitboard occupied = combined_ ^ square_bb(from) 
        ^ square_bb(to);
    Color stm = side_to_move_;
    Bitboard attackers = attackers_to(to, occupied);
    Bitboard stm_attackers, bb;

    int res = 1;
    while (true) {
        stm = ~stm;
        attackers &= occupied;

        if (!(stm_attackers = attackers & pieces(stm)))
            break;

        if (pinners(~stm) & occupied)
            stm_attackers &= ~blockers_for_king(stm);

        if (!stm_attackers)
            break;

        res ^= 1;

        if ((bb = stm_attackers & pieces(PAWN))) {
            if ((balance = PAWN_VALUE - balance) < res)
                break;

            occupied ^= lss_bb(bb);
            attackers |= attacks_bb<BISHOP>(to, occupied)
                & pieces(BISHOP, QUEEN);
        } else if ((bb = stm_attackers & pieces(KNIGHT))) {
            if ((balance = KNIGHT_VALUE - balance) < res)
                break;

            occupied ^= lss_bb(bb);
        } else if ((bb = stm_attackers & pieces(BISHOP))) {
            if ((balance = BISHOP_VALUE - balance) < res)
                break;

            occupied ^= lss_bb(bb);
            attackers |= attacks_bb<BISHOP>(to, occupied)
                & pieces(BISHOP, QUEEN);
        } else if ((bb = stm_attackers & pieces(ROOK))) {
            if ((balance = ROOK_VALUE - balance) < res)
                break;

            occupied ^= lss_bb(bb);
            attackers |= attacks_bb<ROOK>(to, occupied)
                & pieces(ROOK, QUEEN);
        } else if ((bb = stm_attackers & pieces(QUEEN))) {
            if ((balance = QUEEN_VALUE - balance) < res)
                break;

            occupied ^= lss_bb(bb);
            attackers |= attacks_bb<BISHOP>(to, occupied)
                & pieces(BISHOP, QUEEN);
            attackers |= attacks_bb<ROOK>(to, occupied)
                & pieces(ROOK, QUEEN);
        } else {
            return (attackers & ~pieces(stm)) ? res ^ 1 : res;
        }
    }

    return res;
}

