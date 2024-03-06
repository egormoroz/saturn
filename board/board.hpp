#ifndef BOARD_HPP
#define BOARD_HPP

#include "../primitives/common.hpp"
#include "../primitives/bitboard.hpp"
#include <string_view>
#include <iosfwd>
#include "../nnue/nnue_state.hpp"

class Board {
public:
    Board(StateInfo *si);

    static Board start_pos(StateInfo *si);

    [[nodiscard]] bool setup(Bitboard mask, const Piece *pieces, Color stm, 
            CastlingRights cr, Square en_passant);

    char* get_fen(char *buffer) const;
    bool load_fen(std::string_view fen);

    Move parse_lan(std::string_view lan) const;

    void set_stateinfo(StateInfo *si);
    StateInfo* get_stateinfo() const;

    /*
     * Check that the board contains valid information
     * and everyting is synchronized
     * */
    [[nodiscard]] bool is_valid() const;

    Board do_move(Move m, StateInfo *newst) const;
    Board do_null_move(StateInfo *newst) const;

    /*
     * Used for:
     * 1. Checking moves probed from TT
     * 2. Checking moves from various move ordering structures
     * */
    bool is_valid_move(Move m) const;

    bool see_ge(Move m, int threshold = 0) const;

    bool is_quiet(Move m) const;

    void update_pin_info();

    uint64_t mat_key() const;
    bool is_material_draw() const;
    bool has_nonpawns(Color c) const;

    Bitboard attackers_to(Color atk_side, Square s, Bitboard blockers) const;
    Bitboard attackers_to(Square s, Bitboard blockers) const;

    template<bool Checkers>
    Bitboard slider_blockers(Bitboard sliders, Square s,
            Bitboard &pinners, Bitboard *checkers = nullptr) const;

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
    uint8_t half_moves() const;
    uint8_t plies_from_null() const;

private:
    Bitboard pieces_[PIECE_TYPE_NB];
    Bitboard combined_;
    Bitboard color_combined_[COLOR_NB];

    Bitboard checkers_;
    Bitboard blockers_for_king_[COLOR_NB];
    Bitboard pinners_[COLOR_NB];

    Piece pieces_on_[SQUARE_NB];
    uint64_t mat_key_;

    uint64_t key_;
    CastlingRights castling_;
    Color side_to_move_;
    Square en_passant_;

    uint8_t half_moves_;
    uint8_t plies_from_null_;
    uint8_t full_moves_;

    StateInfo *si_;
};

std::ostream& operator<<(std::ostream& os, const Board &b);

#endif
