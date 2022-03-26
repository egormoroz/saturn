#include "board.hpp"
#include "../movgen/attack.hpp"
#include "../core/eval.hpp"

bool Board::see_ge(Move m, int threshold) const {
    if (type_of(m) != NORMAL)
        return threshold >= 0;

    auto value_on = [this](Square s) { 
        return mg_value[type_of(piece_on(s))];
    };

    Square from = from_sq(m), to = to_sq(m);

    int balance = value_on(to) - threshold;
    if (balance < 0)
        return false;

    balance = value_on(from) - balance;
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
            if ((balance = mg_value[PAWN] - balance) < res)
                break;

            occupied ^= lss_bb(bb);
            attackers |= attacks_bb<BISHOP>(to, occupied)
                & pieces(BISHOP, QUEEN);
        } else if ((bb = stm_attackers & pieces(KNIGHT))) {
            if ((balance = mg_value[KNIGHT] - balance) < res)
                break;

            occupied ^= lss_bb(bb);
        } else if ((bb = stm_attackers & pieces(BISHOP))) {
            if ((balance = mg_value[BISHOP] - balance) < res)
                break;

            occupied ^= lss_bb(bb);
            attackers |= attacks_bb<BISHOP>(to, occupied)
                & pieces(BISHOP, QUEEN);
        } else if ((bb = stm_attackers & pieces(ROOK))) {
            if ((balance = mg_value[ROOK] - balance) < res)
                break;

            occupied ^= lss_bb(bb);
            attackers |= attacks_bb<ROOK>(to, occupied)
                & pieces(ROOK, QUEEN);
        } else if ((bb = stm_attackers & pieces(QUEEN))) {
            if ((balance = mg_value[QUEEN] - balance) < res)
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

