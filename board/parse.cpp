#include "board.hpp"
#include <cstring>
#include "../parse_helpers.hpp"
#include "../primitives/utility.hpp"
#include "../zobrist.hpp"
#include <charconv>

bool Board::load_fen(std::string_view fen) {
    trim_front(fen);
    StateInfo *si = si_;
    memset(this, 0, sizeof(Board));
    si_ = si;
    si_->reset();

    for (int r = RANK_8; r >= RANK_1; --r) {
        for (int f = FILE_A; f <= FILE_H; ++f) {
            if (fen.empty())
                return false;

            char ch = fen.front();

            Square s = make_square(File(f), Rank(r));
            if (is_digit(fen.front())) {
                f = File(f + ch - '1');
                fen = fen.substr(1);
                continue;
            }

            Piece p = piece_from_str(fen);
            if (!is_ok(p))
                return false;
            put_piece(p, s);

            fen = fen.substr(1);
        }

        if (r != RANK_1) {
            if (fen.empty())
                return false;
            if (fen.front() != '/')
                return false;
            fen = fen.substr(1);
        }
    }

    trim_front(fen);
    if (fen.empty())
        return false;

    switch (to_lower(fen.front())) {
    case 'w': side_to_move_ = WHITE; break;
    case 'b': side_to_move_ = BLACK; break;
    default: return false;
    };

    if (side_to_move_ == BLACK)
        key_ ^= ZOBRIST.side;

    next_word(fen);
    castling_ = castling_from_str(fen);
    key_ ^= ZOBRIST.castling[castling_];

    next_word(fen);
    en_passant_ = square_from_str(fen);
    if (en_passant_ != SQ_NONE)
        key_ ^= ZOBRIST.enpassant[file_of(en_passant_)];

    next_word(fen);
    if (!fen.empty()) {
        auto end = fen.data() + fen.length();
        auto [rest, ec] = std::from_chars(fen.data(), 
                end, half_moves_);

        if (ec == std::errc() && ++rest < end)
            std::from_chars(rest, end, full_moves_);
    }

    update_pin_info();
    assert(is_valid());

    return true;
}

static char PIECE_CH[COLOR_NB][PIECE_TYPE_NB] = {
    { '?', 'P', 'N', 'B', 'R', 'Q', 'K' },
    { '?', 'p', 'n', 'b', 'r', 'q', 'k' },
};

char* Board::get_fen(char *buffer) const {
    for (int r = RANK_8; r >= RANK_1; --r) {
        int empty_counter = 0;
        for (int f = FILE_A; f <= FILE_H; ++f) {
            Square s = make_square(File(f), Rank(r));
            Piece p = piece_on(s);
            if (p == NO_PIECE) {
                ++empty_counter;
                continue;
            }

            if (empty_counter) {
                *buffer++ = '0' + empty_counter;
                empty_counter = 0;
            }

            *buffer++ = PIECE_CH[color_of(p)][type_of(p)];
        }

        if (empty_counter)
            *buffer++ = '0' + empty_counter;
        if (r != RANK_1)
            *buffer++ = '/';
    }

    *buffer++ = ' ';
    *buffer++ = side_to_move() == WHITE ? 'w' : 'b';
    *buffer++ = ' ';

    if (castling_ & WHITE_KINGSIDE)
        *buffer++ = 'K';
    if (castling_ & WHITE_QUEENSIDE)
        *buffer++ = 'Q';
    if (castling_ & BLACK_KINGSIDE)
        *buffer++ = 'k';
    if (castling_ & BLACK_QUEENSIDE)
        *buffer++ = 'q';
    if (!castling_)
        *buffer++ = '-';

    *buffer++ = ' ';

    if (en_passant_ != SQ_NONE) {
        *buffer++ = 'a' + file_of(en_passant_);
        *buffer++ = '1' + rank_of(en_passant_);
    } else {
        *buffer++ = '-';
    }

    *buffer++ = ' ';
    auto [rest, ec] = std::to_chars(buffer, buffer + 10, half_moves_);
    *rest++ = ' ';
    auto [rest2, _] = std::to_chars(rest, rest + 10, full_moves_);

    *rest2++ = 0;
    return rest2;
}

Move Board::parse_lan(std::string_view lan) const {
    return move_from_str(*this, lan);
}

