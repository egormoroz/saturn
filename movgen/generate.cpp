#include "generate.hpp"
#include "../primitives/common.hpp"
#include "../primitives/bitboard.hpp"
#include "../board/board.hpp"
#include "attack.hpp"


namespace {

/*---------------------Pawn moves--------------------*/

ExtMove* make_proms(Square from, Square to, ExtMove *moves) {
    *moves++ = make<PROMOTION>(from, to, KNIGHT);
    *moves++ = make<PROMOTION>(from, to, BISHOP);
    *moves++ = make<PROMOTION>(from, to, ROOK);
    *moves++ = make<PROMOTION>(from, to, QUEEN);
    return moves;
}

bool legal_ep_move(const Board &b, Square from, Square to) {
    Square cap_sq = make_square(file_of(to), rank_of(from));
    Bitboard combined = b.pieces()
        ^ square_bb(cap_sq)
        ^ square_bb(from) 
        ^ square_bb(to);

    Color us = b.side_to_move(), them = ~us;
    Square ksq = b.king_square(us);
    
    Bitboard bb = 0;
    bb |= attacks_bb<BISHOP>(ksq, combined) 
        & b.pieces(them, BISHOP, QUEEN);
    bb |= attacks_bb<ROOK>(ksq, combined)
        & b.pieces(them, ROOK, QUEEN);

    return !bb;
}

template<GenType T, bool IN_CHECK>
ExtMove* pawn_legals(const Board &b, ExtMove *moves) {
    Color us = b.side_to_move(), them = ~us;
    Square ksq = b.king_square(us);
    Bitboard our_pawns = b.pieces(us, PAWN);
    if (!(T & TACTICAL))
        our_pawns &= ~relative_rank_bb(us, RANK_7);

    Bitboard my_r3 = relative_rank_bb(us, RANK_3),
             my_r8 = relative_rank_bb(us, RANK_8);

    Bitboard check_mask = ~Bitboard(0);
    if (IN_CHECK) {
        check_mask = between_bb(ksq, lsb(b.checkers())) 
            | b.checkers();
    }

    Bitboard pinned = b.blockers_for_king(us);
    Bitboard pawns = our_pawns & ~pinned;

    //May be 2 loops: first for < 7th rank, second for 7th rank?
    while (pawns) {
        Square from = pop_lsb(pawns);

        Bitboard dsts = pawn_pushes_bb(us, from) & ~b.pieces();
        if (!(T & NON_TACTICAL))
            dsts &= my_r8;
        if (T & NON_TACTICAL) {
            Bitboard bb = (my_r3 & b.pieces()) & ~square_bb(from);
            bb = (bb << 8) | (bb >> 8);
            dsts &= ~bb;
        }

        if (T & TACTICAL)
            dsts |= pawn_attacks_bb(us, from) & b.pieces(them);

        if (IN_CHECK)
            dsts &= check_mask;

        Bitboard bb = dsts & my_r8;
        while ((T & TACTICAL) && bb)
            moves = make_proms(from, pop_lsb(bb), moves);

        bb = dsts & ~my_r8;
        while (bb)
            *moves++ = make_move(from, pop_lsb(bb));
    }

    if (!IN_CHECK) {
        pawns = our_pawns & pinned;
        while (pawns) {
            Square from = pop_lsb(pawns);

            Bitboard dsts = 0;
            if (T & NON_TACTICAL) {
                dsts |= pawn_pushes_bb(us, from) & ~b.pieces();
                Bitboard bb = (my_r3 & b.pieces()) & ~square_bb(from);
                bb = (bb << 8) | (bb >> 8);
                dsts &= ~bb;
            }

            if (T & TACTICAL)
                dsts |= pawn_attacks_bb(us, from) & b.pieces(them);

            dsts &= line_bb(ksq, from);
            
            Bitboard bb = dsts & my_r8;
            while ((T & TACTICAL) && bb)
                moves = make_proms(from, pop_lsb(bb), moves);

            bb = dsts & ~my_r8;
            while (bb)
                *moves++ = make_move(from, pop_lsb(bb));
        }
    }

    //branchy boiii
    Square ep = b.en_passant();
    if ((T & TACTICAL) && ep != SQ_NONE) {
        Square to = ep;
        Bitboard rbb = relative_rank_bb(us, RANK_5),
            fbb = adjacent_files_bb(file_of(to));
        Bitboard bb = our_pawns & rbb & fbb;
        while (bb) {
            Square from = pop_lsb(bb);
            if (legal_ep_move(b, from, to))
                *moves++ = make<EN_PASSANT>(from, to);
        }
    }

    return moves;
}

/*----------------End of pawn moves------------------*/


/*-------------------Knight moves--------------------*/

template<GenType T, bool IN_CHECK>
ExtMove* knight_legals(const Board &b, ExtMove *moves) {
    Color us = b.side_to_move(), them = ~us;

    Bitboard our_knights = b.pieces(us, KNIGHT);
    Bitboard pinned = b.blockers_for_king(us);
    Square ksq = b.king_square(us);

    Bitboard mask = 0;
    if (T & TACTICAL)
        mask |= b.pieces(them);
    if (T & NON_TACTICAL)
        mask |= ~b.pieces();
    if (IN_CHECK)
        mask &= between_bb(ksq, lsb(b.checkers()))
            | b.checkers();

    Bitboard bb = our_knights & ~pinned;
    while (bb) {
        Square from = pop_lsb(bb);
        Bitboard dsts = attacks_bb<KNIGHT>(from) & mask;
        while (dsts)
            *moves++ = make_move(from, pop_lsb(dsts));
    }

    return moves;
}

/*----------------End of knight moves----------------*/


/*-------------------Slider moves--------------------*/

template<GenType T, PieceType P, bool IN_CHECK>
ExtMove* slider_legals(const Board &b, ExtMove *moves) {
    static_assert(P == BISHOP || P == ROOK || P == QUEEN);

    Color us = b.side_to_move(), them = ~us;

    Bitboard our_sliders = b.pieces(us, P);
    Square ksq = b.king_square(us);
    Bitboard pinned = b.blockers_for_king(us);

    Bitboard mask = 0;
    if (T & TACTICAL)
        mask |= b.pieces(them);
    if (T & NON_TACTICAL)
        mask |= ~b.pieces();
    if (IN_CHECK)
        mask &= between_bb(ksq, lsb(b.checkers()))
            | b.checkers();

    Bitboard bb = our_sliders & ~pinned;
    while (bb) {
        Square from = pop_lsb(bb);
        Bitboard dsts = attacks_bb<P>(from, b.pieces()) & mask;
        while (dsts)
            *moves++ = make_move(from, pop_lsb(dsts));
    }

    if (!IN_CHECK) {
        bb = our_sliders & pinned;
        while (bb) {
            Square from = pop_lsb(bb);
            Bitboard dsts = attacks_bb<P>(from, b.pieces())
                & line_bb(from, ksq) & mask;
            while (dsts)
                *moves++ = make_move(from, pop_lsb(dsts));
        }
    }

    return moves;
}

/*--------------- End of slider moves----------------*/


/*--------------------King moves--------------------*/

bool legal_king_move(const Board &b, Square to) {
    Color us = b.side_to_move(), them = ~us;

    Bitboard kbb = b.pieces(us, KING);
    Bitboard combined = b.pieces() ^ kbb;

    return !b.attackers_to(them, to, combined);
}


template<GenType T, bool IN_CHECK>
ExtMove* king_legals(const Board &b, ExtMove *moves) {
    Color us = b.side_to_move(), them = ~us;
    Square from = b.king_square(us);

    Bitboard mask = 0;
    if (T & TACTICAL)
        mask |= b.pieces(them);
    if (T & NON_TACTICAL)
        mask |= ~b.pieces();

    Bitboard bb = attacks_bb<KING>(from) & mask;
    while (bb) {
        Square to = pop_lsb(bb);
        if (legal_king_move(b, to))
            *moves++ = make_move(from, to);
    }

    if (IN_CHECK)
        return moves;
    if (!(T & NON_TACTICAL))
        return moves;

    CastlingRights cr = b.castling();
    Bitboard kingside_mask = KINGSIDE_MASK[us];
    Bitboard queenside_mask = QUEENSIDE_MASK[us];

    if (cr & kingside_rights(us) && !(b.pieces() & kingside_mask)) {
        Square middle = Square(from + 1), right = Square(from + 2);
        if (legal_king_move(b, middle) && legal_king_move(b, right))
            *moves++ = make<CASTLING>(from, right);
    }
    
    if (cr & queenside_rights(us) && !(b.pieces() & queenside_mask)) {
        Square middle = Square(from - 1), left = Square(from - 2);
        if (legal_king_move(b, middle) && legal_king_move(b, left))
            *moves++ = make<CASTLING>(from, left);
    }

    return moves;
}

/*-----------------End of king moves----------------*/

} //namespace

template ExtMove* generate<TACTICAL>(const Board&, ExtMove*);
template ExtMove* generate<NON_TACTICAL>(const Board&, ExtMove*);
template ExtMove* generate<LEGAL>(const Board&, ExtMove*);

template<GenType T>
ExtMove* generate(const Board &b, ExtMove *moves) {
    if (!b.checkers()) {
        moves = pawn_legals<T, false>(b, moves);
        moves = knight_legals<T, false>(b, moves);
        moves = slider_legals<T, BISHOP, false>(b, moves);
        moves = slider_legals<T, ROOK, false>(b, moves);
        moves = slider_legals<T, QUEEN, false>(b, moves);
        moves = king_legals<T, false>(b, moves);
    } else if (popcnt(b.checkers()) == 1) {
        moves = pawn_legals<T, true>(b, moves);
        moves = knight_legals<T, true>(b, moves);
        moves = slider_legals<T, BISHOP, true>(b, moves);
        moves = slider_legals<T, ROOK, true>(b, moves);
        moves = slider_legals<T, QUEEN, true>(b, moves);
        moves = king_legals<T, true>(b, moves);
    } else {
        moves = king_legals<T, true>(b, moves);
    }

    return moves;
}

