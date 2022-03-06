#ifndef BOARD_HPP
#define BOARD_HPP

#include "../primitives/common.hpp"
#include "../primitives/bitboard.hpp"
#include <string_view>
#include <iosfwd>

class Board {
public:
    Board() = default;

    bool load_fen(std::string_view fen);

    /*
     * Check that the board contains valid information
     * and everyting is synchronized
     * */
    void validate() const;

    Board do_move(Move m) const;
    Board do_null_move() const;

    /*
     * Used for:
     * 1. Checking moves probed from TT
     * 2. Checking moves from various move ordering structures
     * */
    bool is_valid_move(Move m) const;

    // Checks if the see value of the move is >= 0
    bool nonneg_see(Move m) const;

    void update_pin_info();

    Bitboard attackers_to(Color atk_side, Square s, Bitboard blockers) const;
    Bitboard attackers_to(Square s, Bitboard blockers) const;

    template<bool Checkers>
    Bitboard slider_blockers(Bitboard sliders, Square s,
            Bitboard &pinners, Bitboard *checkers = nullptr) const;

    //so far put_piece and remove_piece are identical
    //but that might change in the future
    void put_piece(Piece p, Square s);
    void remove_piece(Square s);

    Bitboard pieces() const;
    Bitboard pieces(Color c) const;

    Bitboard pieces(PieceType pt) const;
    Bitboard pieces(PieceType pt1, PieceType pt2) const;

    Bitboard pieces(Color c, PieceType pt) const;
    Bitboard pieces(Color c, PieceType pt1, PieceType pt2) const;

    Piece piece_on(Square s) const;

    Bitboard checkers() const;
    Bitboard blockers_for_king(Color c) const;
    Bitboard pinners(Color c) const;

    Square king_square(Color c) const;

    Color side_to_move() const;
    Square en_passant() const;
    CastlingRights castling() const;

    uint64_t key() const;

    //returns number of moves since last capture/pawn move
    int fifty_rule() const;

private:
    Bitboard pieces_[PIECE_TYPE_NB];
    Bitboard combined_;
    Bitboard color_combined_[COLOR_NB];

    Bitboard checkers_;
    Bitboard blockers_for_king_[COLOR_NB];
    Bitboard pinners_[COLOR_NB];

    Piece pieces_on_[SQUARE_NB];

    uint64_t key_;
    CastlingRights castling_;

    Color side_to_move_;
    Square en_passant_;
    int fifty_;
};

std::ostream& operator<<(std::ostream& os, const Board &b);

#endif
