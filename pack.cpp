#include "pack.hpp"
#include "board/board.hpp"
#include "movgen/attack.hpp"
#include "zobrist.hpp"
#include "movgen/generate.hpp"
#include <vector>
#include <fstream>
#include <cstring>

template<typename T>
void unsigned_to_bytes(T x, uint8_t *bytes) {
    for (size_t i = 0; i < sizeof(T); ++i)
        bytes[i] = (x >> (8*i)) & 0xFF;
}

// TODO: return using an lvalue reference
template<typename T>
T bytes_to_unsigned(const uint8_t *bytes) {
    T x{};
    for (size_t i = 0; i < sizeof(T); ++i)
        x |= T(bytes[i]) << (8*i);
    return x;
}

enum PackedPiece : uint8_t {
    PPW_PAWN, PPB_PAWN,
    PPW_KNIGHT, PPB_KNIGHT,
    PPW_BISHOP, PPB_BISHOP,
    PPW_ROOK, PPB_ROOK,
    PPW_QUEEN, PPB_QUEEN,
    PPW_KING, PPB_KING,

    // pawn with ep square behind
    PP_ENPASSANT, 

    // rooks with respective colors and CRs; could be reduced to a single value
    PP_WCR_ROOK, PP_BCR_ROOK,

    // used for black king instead, if its black to move
    PP_BSTM_KING,
};

constexpr inline Piece unpack_piece(PackedPiece p) {
    return make_piece(Color(p % 2), PieceType(1 + p / 2));
}

constexpr inline PackedPiece pack_piece(Piece p) {
    return PackedPiece((type_of(p) - 1) * 2 + color_of(p));
}

PackedBoard pack_board(const Board &b) {
    PackedBoard pb{};
    pb.pc_mask = b.pieces();

    Square ep = b.en_passant();
    if (is_ok(ep))
        ep = sq_forward(~b.side_to_move(), ep);

    CastlingRights cr = b.castling();
    Bitboard bb = pb.pc_mask;
    for (int pc_idx = 0; bb; ++pc_idx) {
        Square sq = pop_lsb(bb);
        File file = file_of(sq);
        Piece p = b.piece_on(sq);

        uint8_t nibble = 0;

        if (sq == ep)
            nibble = PP_ENPASSANT;

        if (p == W_ROOK && ((file == FILE_A && cr & WHITE_QUEENSIDE) 
                    | (file == FILE_H && cr & WHITE_KINGSIDE)))
            nibble = PP_WCR_ROOK;

        if (p == B_ROOK && ((file == FILE_A && cr & BLACK_QUEENSIDE) 
                    | (file == FILE_H && cr & BLACK_KINGSIDE)))
            nibble = PP_BCR_ROOK;

        if (p == B_KING && b.side_to_move() == BLACK)
            nibble = PP_BSTM_KING;

        if (!nibble)
            nibble = pack_piece(p);

        if (pc_idx % 2)
            nibble = nibble << 4;
        pb.pc_list[pc_idx / 2] |= nibble;
    }

    return pb;
}

uint64_t packed_hash(const PackedBoard &pb) {
    uint64_t k = 0;

    uint64_t mask = pb.pc_mask;
    CastlingRights cr = NO_CASTLING;
    Color stm = WHITE;

    Square ep = SQ_NONE;

    for (int pc_idx = 0; mask; ++pc_idx) {
        Square sq = pop_lsb(mask);
        File file = file_of(sq);
        uint8_t nibble = pb.pc_list[pc_idx / 2];
        nibble = pc_idx % 2 ? nibble >> 4 : nibble & 0xF;

        if (nibble == PP_BSTM_KING) {
            stm = BLACK;
            nibble = PPB_KING;
        }
        
        if (nibble == PP_ENPASSANT) {
            Color pc_color = rank_of(sq) == RANK_4 ? WHITE : BLACK;
            ep = sq_backward(pc_color, sq);
            nibble = pc_color == WHITE ? PPW_PAWN : PPB_PAWN;
        }

        if (nibble == PP_WCR_ROOK) {
            cr = CastlingRights(cr | (file == FILE_A ? WHITE_QUEENSIDE 
                        : WHITE_KINGSIDE));
            nibble = PPW_ROOK;
        }

        if (nibble == PP_BCR_ROOK) {
            cr = CastlingRights(cr | (file == FILE_A ? BLACK_QUEENSIDE 
                        : BLACK_KINGSIDE));
            nibble = PPB_ROOK;
        }

        Piece p = unpack_piece(PackedPiece(nibble));
        if (nibble <= PPB_KING)
            k ^= ZOBRIST.psq[p][sq];
    }

    if (is_ok(ep))
        k ^= ZOBRIST.enpassant[file_of(ep)];

    k ^= ZOBRIST.castling[cr];

    if (stm == BLACK)
        k ^= ZOBRIST.side;

    return k;
}

bool unpack_board(const PackedBoard &pb, Board &b) {
    uint64_t hash = 0;
    uint64_t mask = pb.pc_mask;
    CastlingRights cr = NO_CASTLING;
    Color stm = WHITE;

    Square ep = SQ_NONE;

    if (popcnt(mask) > 32)
        return false;

    Piece pc_list[32];
    for (int pc_idx = 0; mask; ++pc_idx) {
        Square sq = pop_lsb(mask);
        File file = file_of(sq);
        uint8_t nibble = pb.pc_list[pc_idx / 2];
        nibble = pc_idx % 2 ? nibble >> 4 : nibble & 0xF;

        if (nibble == PP_BSTM_KING) {
            stm = BLACK;
            nibble = PPB_KING;
        }
        
        if (nibble == PP_ENPASSANT) {
            Color pc_color = rank_of(sq) == RANK_4 ? WHITE : BLACK;
            ep = sq_backward(pc_color, sq);
            nibble = pc_color == WHITE ? PPW_PAWN : PPB_PAWN;
        }

        if (nibble == PP_WCR_ROOK) {
            cr = CastlingRights(cr | (file == FILE_A ? WHITE_QUEENSIDE 
                        : WHITE_KINGSIDE));
            nibble = PPW_ROOK;
        }

        if (nibble == PP_BCR_ROOK) {
            cr = CastlingRights(cr | (file == FILE_A ? BLACK_QUEENSIDE 
                        : BLACK_KINGSIDE));
            nibble = PPB_ROOK;
        }

        if (nibble <= PPB_KING) {
            Piece p = unpack_piece(PackedPiece(nibble));
            pc_list[pc_idx] = p;
            hash ^= ZOBRIST.psq[p][sq];
        }
    }

    if (is_ok(ep))
        hash ^= ZOBRIST.enpassant[file_of(ep)];

    hash ^= ZOBRIST.castling[cr];

    if (stm == BLACK)
        hash ^= ZOBRIST.side;

    return b.setup(pb.pc_mask, pc_list, stm, cr, ep)
        && hash == b.key();
}

void merge_packed_games(const char **game_fnames, 
        const char **hash_fnames, int n_games,
        const char *fout_games, const char *fout_hash) 
{
    constexpr size_t buf_size = 1024*1024;
    std::vector<char> buffer(buf_size);

    std::ofstream fout(fout_games, std::ios::binary);
    for (int i = 0; i < n_games; ++i) {
        std::ifstream fin(game_fnames[i], std::ios::binary);
        fin.seekg(0, std::ios::end);
        size_t file_size = fin.tellg();
        fin.seekg(0);
        while (file_size) {
            size_t chunk_size = std::min(buf_size, file_size);
            fin.read(buffer.data(), chunk_size);
            fout.write(buffer.data(), chunk_size);

            file_size -= chunk_size;
        }
    }

    uint64_t hash = 0, t;
    for (int i = 0; i < n_games; ++i) {
        std::ifstream fin(hash_fnames[i]);
        fin >> t;
        hash ^= t;
    }

    fout.close();
    fout.open(fout_hash);
    fout << hash << '\n';
}


void merge_packed_games2(const char **game_fnames, int n_games, const char *fout_games) {
    ChunkHead head;
    ChainReader2 cr;

    std::vector<char> buffer(PACK_CHUNK_SIZE + CHUNK_PADDING);
    std::vector<PosChain> leftovers;

    std::ofstream fout(fout_games, std::ios::binary);
    for (int i = 0; i < n_games; ++i) {
        std::ifstream fin(game_fnames[i], std::ios::binary);

        while (true) {
            if (fin.read(buffer.data(), PACK_CHUNK_SIZE)) {
                assert(fin.gcount() == PACK_CHUNK_SIZE);
                fout.write(buffer.data(), PACK_CHUNK_SIZE);
                continue;
            }

            size_t n_read = fin.gcount();
            if (n_read < ChunkHead::SIZE) break;

            head.from_bytes((const uint8_t*)buffer.data());
            if (head.SIZE + head.body_size > PACK_CHUNK_SIZE) break;

            const uint8_t *ptr = (const uint8_t*)buffer.data() + head.SIZE;
            size_t buf_size = head.body_size + CHUNK_PADDING;

            for (uint32_t i = 0; i < head.n_chains; ++i) {
                if (!is_ok(cr.start_new_chain(ptr, buf_size))) break;

                leftovers.resize(leftovers.size() + 1);
                PosChain &pc = leftovers.back();

                pc.start = pack_board(cr.board);
                pc.result = cr.result;

                do {
                    pc.seq[pc.n_moves++] = { cr.move, cr.score };
                } while (is_ok(cr.next()));

                buf_size -= cr.tellg();
                ptr += cr.tellg();
            }

            break;
        }
    }

    ChainWriter cw(fout);
    for (auto &pc: leftovers)
        cw.write(pc);
}

// TODO: consider using pext.
// https://stackoverflow.com/questions/7669057/find-nth-set-bit-in-an-int/7671563#7671563
constexpr inline Bitboard nth_lsb(Bitboard v, int n) {
   for (int i=0; i < n; i++) {
      v &= v-1; // remove the least significant bit
   }
   return v & ~(v-1); // extract the least significant bit
}

inline uint8_t get_sq_index(Bitboard mask, Square sq) {
    return popcnt((square_bb(sq) - 1) & mask);
}

static void write_square(BitWriter &bw, Bitboard mask, Square sq) {
    int n = popcnt(mask);
    if (n > 1) {
        int idx = get_sq_index(mask, sq);
        bw.write(idx, msb(n - 1) + 1);
    }
}

static Square read_square(BitReader &br, Bitboard mask) {
    int n = popcnt(mask);
    if (n > 1) {
        uint8_t idx = br.read<uint8_t>(msb(n - 1) + 1);
        uint64_t b = nth_lsb(mask, idx);
        Square sq = lsb(b);
        return sq;
    } else {
        return lsb(mask);
    }
}

static void write_pawn_move(BitWriter &bw, const Board &b, Move m) {
    Square from = from_sq(m), ep = b.en_passant();
    Color us = b.side_to_move(), them = ~us;
    Bitboard ep_bb = is_ok(ep) ? square_bb(ep) : 0;

    Bitboard dst = pawn_pushes_bb(us, from) & ~b.pieces(them);
    dst |= pawn_attacks_bb(us, from) & (b.pieces(them) | ep_bb);

    if (relative_rank(us, from) == RANK_7)
        bw.write(prom_type(m) - KNIGHT, 2);

    write_square(bw, dst, to_sq(m));
}

static Move read_pawn_move(BitReader &br, const Board &b, Square from) {
    Square ep = b.en_passant();
    Color us = b.side_to_move(), them = ~us;
    Bitboard ep_bb = is_ok(ep) ? square_bb(ep) : 0;

    Bitboard dst = pawn_pushes_bb(us, from) & ~b.pieces(them);
    dst |= pawn_attacks_bb(us, from) & (b.pieces(them) | ep_bb);

    uint8_t prom_type = 0;
    if (relative_rank(us, from) == RANK_7)
        prom_type = (uint8_t)br.read<uint8_t>(2) + KNIGHT;

    Square to = read_square(br, dst);

    if (prom_type)
        return make<PROMOTION>(from, to, PieceType(prom_type));
    else if (to == ep)
        return make<EN_PASSANT>(from, to);
    else
        return make_move(from, to);
}

static void write_king_move(BitWriter &bw, const Board &b, Move m) {
    Square from = from_sq(m);
    Color us = b.side_to_move();

    Bitboard dst = attacks_bb<KING>(from) & ~b.pieces(us);
    CastlingRights cr = b.castling();

    int n_dsts = popcnt(dst);
    int n_crs = popcnt(cr & (kingside_rights(us) | queenside_rights(us)));
    int idx_max = n_dsts + n_crs - 1;

    int idx = 0;
    if (type_of(m) == CASTLING) {
        idx = n_dsts;
        if (n_crs == 2 && file_of(to_sq(m)) == FILE_C)
            idx++; // add one for the O-O-O
    } else {
        idx = get_sq_index(dst, to_sq(m));
    }

    if (idx_max > 0)
        bw.write(idx, msb(idx_max) + 1);
}

static Move read_king_move(BitReader &br, const Board &b, Square from) {
    Color us = b.side_to_move();

    Bitboard dst = attacks_bb<KING>(from) & ~b.pieces(us);
    CastlingRights cr = b.castling();

    int n_dsts = popcnt(dst);
    int n_crs = popcnt(cr & (kingside_rights(us) | queenside_rights(us)));
    int idx_max = n_dsts + n_crs - 1;

    int idx = idx_max > 0 ? br.read<uint8_t>(msb(idx_max) + 1) : 0;
    if (idx < n_dsts) {
        Square to = lsb(nth_lsb(dst, idx));
        return make_move(from, to);
    } else {
        bool castle_long = false;
        if ((n_crs == 2 && idx == idx_max) || (n_crs == 1 && (cr & queenside_rights(us))))
            castle_long = true;
        Square to = castle_long ? Square(from - 2) : Square(from + 2);
        return make<CASTLING>(from, to);
    }
}

static void write_move(BitWriter &bw, const Board &b, Move m) {
    Color us = b.side_to_move();
    Square from = from_sq(m);
    PieceType pt = type_of(b.piece_on(from));

    Bitboard mask;
    if (pt == KNIGHT) {
        mask = attacks_bb<KNIGHT>(from);
    } else {
        assert(pt >= BISHOP && pt <= QUEEN);
        mask = attacks_bb(pt, from, b.pieces()) & ~b.pieces(us);
    }
    write_square(bw, mask, to_sq(m));
}

static Move read_move(BitReader &br, const Board &b, Square from) {
    Color us = b.side_to_move();
    PieceType pt = type_of(b.piece_on(from));

    Bitboard mask;
    if (pt == KNIGHT) {
        mask = attacks_bb<KNIGHT>(from);
    } else {
        assert(pt >= BISHOP && pt <= QUEEN);
        mask = attacks_bb(pt, from, b.pieces()) & ~b.pieces(us);
    }

    Square to = read_square(br, mask);
    return make_move(from, to);
}

// Variable width with block of 4 bit and 1 bit for extenstion.
static void write_int(BitWriter &bw, int16_t x) {
    constexpr int block_size = 4;
    constexpr uint16_t block_mask = 0b1111;

    uint16_t ux = abs(x);
    ux = ux << 1 | (x >= 0 ? 0 : 1);

    while (true) {
        bw.write(ux & block_mask, block_size);
        ux >>= block_size;

        if (ux) bw.write(1, 1);
        else break;
    }
    bw.write(0, 1);
}

static int16_t read_int(BitReader &br) {
    constexpr int block_size = 4;

    int off = 0;
    uint16_t x = 0;
    do {
        x |= br.read<uint16_t>(block_size) << off;
        off += block_size;

    } while (br.read<uint8_t>(1));

    int16_t sign = (x & 1) == 0 ? 1 : -1;
    return int16_t(x >> 1) * sign;
}

std::pair<uint8_t*, uint64_t> PosChain::write_to_buf(uint8_t *buf, size_t buf_size) const {
    assert(buf_size >= 2 * MAX_PLIES);
    static_assert(sizeof(uint8_t) == 1);

    const uint8_t * const buf_end = buf + buf_size;

    unsigned_to_bytes(start.pc_mask, buf);
    buf += sizeof(start.pc_mask);

    memcpy(buf, start.pc_list, sizeof(start.pc_list));
    buf += sizeof(start.pc_list);

    uint16_t len_and_result = (n_moves << 2) | result;
    unsigned_to_bytes(len_and_result, buf);
    buf += 2;

    memset(buf, 0, buf_end - buf);
    BitWriter bw;
    bw.data = buf;
    bw.cursor = 0;

    Board b;
    bool _ = unpack_board(start, b);
    assert(_);

    uint64_t hash = b.key();

    int16_t prev_score = 0;
    for (uint16_t i = 0; i < n_moves; ++i) {
        Move m = seq[i].move;
        write_square(bw, b.pieces(b.side_to_move()), from_sq(m));

        Piece p = b.piece_on(from_sq(m));
        PieceType pt = type_of(p);
        if (pt == PAWN)
            write_pawn_move(bw, b, m);
        else if (pt == KING)
            write_king_move(bw, b, m);
        else 
            write_move(bw, b, m);

        int16_t diff = -prev_score - seq[i].score;
        write_int(bw, diff);
        prev_score = seq[i].score;

        b = b.do_move(m);
        hash ^= b.key();
    }

    buf += (bw.cursor + 7) / 8;

    return { buf, hash };
}

void PosChain::write_to_stream(std::ostream &os) const {
    uint8_t buf[MAX_PLIES * 2];
    auto [buf_end, hash] = write_to_buf(buf, sizeof(buf));
    os.write((const char*)buf, buf_end - buf);
}

bool PosChain::load_from_stream(std::istream &is) {
    uint8_t buf[MAX_PLIES * 2];
    if (!is.read((char*)buf, sizeof(start.pc_mask)))
        return false;
    start.pc_mask = bytes_to_unsigned<uint64_t>(buf);

    if (!is.read((char*)start.pc_list, sizeof(start.pc_list)))
        return false;

    if (!is.read((char*)buf, 2))
        return false;
    uint16_t len_and_result = bytes_to_unsigned<uint16_t>(buf);

    result = len_and_result & 3;
    n_moves = len_and_result >> 2;
    assert(n_moves <= MAX_PLIES);

    Board b;
    if (!unpack_board(start, b))
        return false;

    size_t offset = is.tellg();
    is.read((char*)buf, sizeof(buf));
    size_t read = is.gcount();

    BitReader br;
    br.data = buf;
    br.cursor = 0;

    int16_t score = 0;
    for (int i = 0; i < n_moves; ++i) {
        Square sq = read_square(br, b.pieces(b.side_to_move()));
        PieceType pt = type_of(b.piece_on(sq));

        Move m;
        if (pt == PAWN)
            m = read_pawn_move(br, b, sq);
        else if (pt == KING)
            m = read_king_move(br, b, sq);
        else
            m = read_move(br, b, sq);

        seq[i].move = m;
        int16_t diff = read_int(br);
        score = -score - diff;
        seq[i].score = score;

        if (!b.is_valid_move(m))
            return false;

        b = b.do_move(m);
    }

    if (8 * read >= br.cursor) {
        is.clear();
        is.seekg(offset + (br.cursor + 7) / 8);
        return true;
    }
    return false;
}

int recover_pack(const char *fname_in, const char *fname_out, const char *hash_out) {
    std::ifstream fin(fname_in, std::ios::binary);
    std::ofstream fout(fname_out, std::ios::binary);

    uint64_t cum_hash = 0;
    int n_pos = 0;

    ChainReader reader;
    PackResult pr;

    char buf[PosChain::MAX_PACKED_SIZE];

    while (fin) {
        size_t start = fin.tellg();
        pr = reader.start_new_chain(fin);
        if (pr == PackResult::END_OF_FILE || pr == PackResult::UNEXPECTED_EOF)
            break;

        if (is_ok(pr)) {
            uint64_t h = reader.board.key();
            while (is_ok(pr = reader.next(fin)))
                h ^= reader.board.key();

            if (pr == PackResult::END_OF_CHAIN)  {
                size_t n_bytes = size_t(fin.tellg()) - start;
                fin.clear();
                fin.seekg(start);
                fin.read(buf, n_bytes);
                fout.write(buf, n_bytes);
                n_pos += reader.n_moves;

                h ^= reader.board.do_move(reader.move, &reader.si).key();
                cum_hash ^= h;
                continue;
            }
        }

        fin.seekg(start + 1);
    }

    fout.close();
    fout.open(hash_out);
    fout << cum_hash;

    return n_pos;
}

bool validate_packed_games(const char *fname) {
    std::ifstream fin(fname, std::ios::binary);

    ChainReader reader;
    PackResult pr;

    while (is_ok(pr = reader.start_new_chain(fin))) {
        while (is_ok(pr = reader.next(fin)));

        if (pr != PackResult::END_OF_CHAIN)
            return false;
    }

    return pr == PackResult::END_OF_FILE;
}


bool validate_packed_games(const char *fname, uint64_t expected_cum_hash) {
    std::ifstream fin(fname, std::ios::binary);

    uint64_t cum_hash = 0;
    ChainReader reader;
    PackResult pr;

    while (is_ok(pr = reader.start_new_chain(fin))) {
        cum_hash ^= reader.board.key();

        while (is_ok(pr = reader.next(fin)))
            cum_hash ^= reader.board.key();

        if (pr != PackResult::END_OF_CHAIN)
            return false;

        cum_hash ^= reader.board.do_move(reader.move, &reader.si).key();
    }

    return expected_cum_hash == cum_hash;
}

bool validate_packed_games(const char *fname, const char *hashes_fname) {
    std::ifstream fin(hashes_fname);
    if (!fin) {
#ifndef NDEBUG
        printf("failed to load hash\n");
#endif
        return false;
    }

    uint64_t hash;
    fin >> hash;
    fin.close();
    return validate_packed_games(fname, hash);
}


ChainReader::ChainReader()
    : board(&si)
{}

PackResult ChainReader::start_new_chain(std::istream &is) {
    PackedBoard start;

    if (!is.read((char*)buf, sizeof(start.pc_mask)))
        return PackResult::END_OF_FILE;
    start.pc_mask = bytes_to_unsigned<uint64_t>(buf);

    if (!is.read((char*)start.pc_list, sizeof(start.pc_list)))
        return PackResult::UNEXPECTED_EOF;

    if (!is.read((char*)buf, 2))
        return PackResult::UNEXPECTED_EOF;
    uint16_t len_and_result = bytes_to_unsigned<uint16_t>(buf);

    result = len_and_result & 3;
    n_moves = len_and_result >> 2;
    move_idx = 0;
    move = MOVE_NONE;
    score = 0;

    if (n_moves > MAX_PLIES)
        return PackResult::INVALID_HEADER;
    if (result > 2)
        return PackResult::INVALID_HEADER;

    if (!unpack_board(start, board))
        return PackResult::INVALID_BOARD;

    memset(buf, 0, sizeof(buf));
    br.data = buf;
    br.cursor = 0;

    is_off_start = is.tellg();
    is.read((char*)buf, sizeof(buf));
    is_bytes_read = is.gcount();

    // read the first entry
    read_movescore();
    if (!board.is_valid_move(move))
        return PackResult::INVALID_MOVE;

    return 8 * is_bytes_read >= br.cursor 
        ? PackResult::OK 
        : PackResult::UNEXPECTED_EOF;
}

PackResult ChainReader::next(std::istream &is) {
    // don't forget to fix the stream position
    if (move_idx == n_moves)
        adjust_istream_pos(is);
    if (move_idx >= n_moves)
        return PackResult::END_OF_CHAIN;

    assert(board.is_valid_move(move)); // we already checked so it's an assert
    board = board.do_move(move, &si);
    read_movescore();

    if (!board.is_valid_move(move))
        return PackResult::INVALID_MOVE;

    // This entry isn't valid if we couldn't read enough bytes
    return 8 * is_bytes_read >= br.cursor 
        ? PackResult::OK 
        : PackResult::UNEXPECTED_EOF;
}

bool ChainReader::skip_invalid_data(std::istream &is) {
    size_t off = is.tellg();

    while (is) {
        PackResult pr = start_new_chain(is);
        if (pr == PackResult::UNEXPECTED_EOF) 
            return false;

        if (is_ok(pr)) {
            while (is_ok(pr = next(is)));
            if (pr == PackResult::END_OF_CHAIN) {
                is.clear();
                is.seekg(off);
                return true;
            }
        }

        is.seekg(++off);
    }

    return false;
}


void ChainReader::adjust_istream_pos(std::istream &is) const {
    // In start_new_chain we read a bug chunk of bytes: most likely more than we need, so
    // we must adjust the stream position manually. It isn't pretty, but it's simple.
    is.clear();
    is.seekg(is_off_start + (br.cursor + 7) / 8);
}

void ChainReader::read_movescore() {
    Square sq = read_square(br, board.pieces(board.side_to_move()));
    PieceType pt = type_of(board.piece_on(sq));

    if (pt == PAWN)
        move = read_pawn_move(br, board, sq);
    else if (pt == KING)
        move = read_king_move(br, board, sq);
    else
        move = read_move(br, board, sq);

    int16_t diff = read_int(br);
    score = -score - diff;

    ++move_idx;
}


void PackIndex::write_to_stream(std::ostream &os) const {
    constexpr size_t n = sizeof(size_t);

    uint8_t buf[sizeof(Block)];
    unsigned_to_bytes(n_blocks, buf);
    os.write((const char*)buf, sizeof(n_blocks));

    for (size_t i = 0; i < n_blocks; ++i) {
        unsigned_to_bytes(blocks[i].off_begin, buf + n * 0);
        unsigned_to_bytes(blocks[i].off_end, buf + n * 1);
        unsigned_to_bytes(blocks[i].n_pos, buf + n * 2);
        os.write((const char*)buf, sizeof(Block));
    }
}

bool PackIndex::load_from_stream(std::istream &is) {
    constexpr size_t n = sizeof(size_t);
    uint8_t buf[sizeof(Block)];

    if (!is.read((char*)buf, n))
        return false;

    n_blocks = bytes_to_unsigned<size_t>(buf);
    if (n_blocks > MAX_BLOCKS)
        return false;

    for (size_t i = 0; i < n_blocks; ++i) {
        if (!is.read((char*)buf, sizeof(Block)))
            return false;
        blocks[i].off_begin = bytes_to_unsigned<size_t>(buf + n * 0);
        blocks[i].off_end = bytes_to_unsigned<size_t>(buf + n * 1);
        blocks[i].n_pos = bytes_to_unsigned<size_t>(buf + n * 2);

        if (blocks[i].off_begin > blocks[i].off_end)
            return false;
    }

    return true;
}

bool create_index(const char *fname_pack, PackIndex &pi) {
    constexpr size_t min_block_size = 1024 * 1024; // 1MB

    std::ifstream fin(fname_pack, std::ios::binary);
    fin.seekg(0, std::ios::end);
    size_t n_file_size = fin.tellg();
    fin.clear();
    fin.seekg(0);

    size_t n_block_size = std::max(min_block_size, n_file_size / PackIndex::MAX_BLOCKS);

    ChainReader cr;
    PackResult pr;
    pi.n_blocks = 0;
    size_t off_begin = 0, n_pos = 0;

    while (is_ok(pr = cr.start_new_chain(fin))) {
        while (is_ok(pr = cr.next(fin)));

        if (pr != PackResult::END_OF_CHAIN)
            return false;

        n_pos += cr.n_moves;
        size_t off_end = std::min(size_t(fin.tellg()), n_file_size);

        if (off_end - off_begin >= n_block_size) {
            PackIndex::Block &blk = pi.blocks[pi.n_blocks++];
            blk.off_begin = off_begin;
            blk.off_end = off_end;
            blk.n_pos = n_pos;

            n_pos = 0;
            off_begin = off_end;
        }
    }

    if (pr != PackResult::END_OF_FILE)
        return false;

    if (n_pos) {
        PackIndex::Block &blk = pi.blocks[pi.n_blocks++];
        blk.off_begin = off_begin;
        blk.off_end = n_file_size;
        blk.n_pos = n_pos;
    }

    return true;
}

bool create_index(const char *fname_pack, const char *fname_index) {
    PackIndex pi;
    if (!create_index(fname_pack, pi))
        return false;

    std::ofstream fout(fname_index, std::ios::binary);
    pi.write_to_stream(fout);
    return bool(fout);
}

void ChunkHead::to_bytes(uint8_t* buf) const{
    unsigned_to_bytes(hash, buf);
    unsigned_to_bytes(n_chains, buf + 8);
    unsigned_to_bytes(body_size, buf + 12);
    unsigned_to_bytes(n_pos, buf + 16);
}

void ChunkHead::from_bytes(const uint8_t* buf) {
    hash = bytes_to_unsigned<uint64_t>(buf);
    n_chains = bytes_to_unsigned<uint32_t>(buf + 8);
    body_size = bytes_to_unsigned<uint32_t>(buf + 12);
    n_pos = bytes_to_unsigned<uint32_t>(buf + 16);
}

ChainWriter::ChainWriter(std::ostream &os)
    : os_(os)
{
    chunk_start_ = os_.tellp();
}

void ChainWriter::write(const PosChain &pc) {
    assert(chunk_off_ <= PACK_CHUNK_SIZE);

    auto [buf_end, hash] = pc.write_to_buf(buf_, sizeof(buf_));

    size_t n_written = buf_end - buf_;
    if (chunk_off_ + n_written > PACK_CHUNK_SIZE)
        finish_chunk(true);

    if (!chunk_off_) {
        char zeros[ChunkHead::SIZE]{};
        os_.write(zeros, ChunkHead::SIZE);
        chunk_off_ += ChunkHead::SIZE;
    }

    chunk_off_ += n_written;

    head_.hash ^= hash;
    head_.n_chains++;
    head_.body_size += (uint32_t)n_written;
    head_.n_pos += pc.n_moves;

    os_.write((const char*)buf_, n_written);
}

ChainWriter::~ChainWriter() {
    if (os_ && head_.n_chains)
        finish_chunk(false);
}

void ChainWriter::finish_chunk(bool pad) {
    assert(chunk_off_ <= PACK_CHUNK_SIZE);

    constexpr size_t block_len = 64;
    char zeros[block_len]{};

    if (pad) {
        const size_t n_pad = PACK_CHUNK_SIZE - chunk_off_;
        for (size_t i = 0; i < n_pad; i += block_len)
            os_.write(zeros, std::min(block_len, n_pad - i));
    }

    head_.to_bytes((uint8_t*)zeros);
    size_t off = os_.tellp();
    os_.seekp(chunk_start_);
    os_.write(zeros, ChunkHead::SIZE);

    os_.seekp(off);

    chunk_start_ = off;
    chunk_off_ = 0;

    head_.hash = 0;
    head_.n_chains = 0;
    head_.body_size = 0;
    head_.n_pos = 0;
}


PackResult ChainReader2::start_new_chain(const uint8_t *buf, size_t buf_size) {
    buf_size_ = buf_size;
    br_.data = buf;

    if (buf_size == 0)
        return PackResult::END_OF_FILE;

    if (buf_size < sizeof(PackedBoard) + 2)
        return PackResult::UNEXPECTED_EOF;

    PackedBoard start;
    start.pc_mask = bytes_to_unsigned<uint64_t>(buf);
    buf += 8;
    memcpy(start.pc_list, buf, sizeof(start.pc_list));
    buf += sizeof(start.pc_list);

    uint16_t len_and_result = bytes_to_unsigned<uint16_t>(buf);
    buf += 2;

    result = len_and_result & 3;
    n_moves = len_and_result >> 2;
    if (n_moves > MAX_PLIES || result > 2)
        return PackResult::INVALID_HEADER;

    if (!unpack_board(start, board))
        return PackResult::INVALID_BOARD;

    move_idx = 0;
    move = MOVE_NONE;
    score = 0;
    br_.cursor = 8 * (buf - br_.data);

    if (!read_movescore())
        return PackResult::UNEXPECTED_EOF;

    return board.is_valid_move(move) ? PackResult::OK : PackResult::INVALID_MOVE;
}

PackResult ChainReader2::next() {
    if (move_idx >= n_moves)
        return PackResult::END_OF_CHAIN;

    board = board.do_move(move);
    if (!read_movescore())
        return PackResult::UNEXPECTED_EOF;

    if (!board.is_valid_move(move))
        return PackResult::INVALID_MOVE;

    return PackResult::OK;
}

bool ChainReader2::read_movescore() {
    if (tellg() + CHUNK_PADDING >= buf_size_)
        return false;

    Square sq = read_square(br_, board.pieces(board.side_to_move()));
    PieceType pt = type_of(board.piece_on(sq));

    if (pt == PAWN)
        move = read_pawn_move(br_, board, sq);
    else if (pt == KING)
        move = read_king_move(br_, board, sq);
    else
        move = read_move(br_, board, sq);

    int16_t diff = read_int(br_);
    score = -score - diff;

    ++move_idx;

    return tellg() + CHUNK_PADDING <= buf_size_;
}

size_t ChainReader2::tellg() const {
    return (br_.cursor + 7) / 8;
}


bool repack_games(const char *fname_in, const char *fname_out) {
    std::ifstream fin(fname_in, std::ios::binary);
    std::ofstream fout(fname_out, std::ios::binary);

    ChainReader reader; // old
    PackResult pr;
    ChainWriter writer(fout); // new

    PosChain pc;

    while (is_ok(pr = reader.start_new_chain(fin))) {
        pc.start = pack_board(reader.board);
        pc.n_moves = 0;
        pc.result = reader.result;


        pc.seq[pc.n_moves++] = { reader.move, reader.score };

        while (is_ok(pr = reader.next(fin)))
            pc.seq[pc.n_moves++] = { reader.move, reader.score };

        if (pr != PackResult::END_OF_CHAIN)
            return false;

        writer.write(pc);
    }

    return true;
}

bool validate_packed_games2(const char *fname, uint64_t &hash_out) {
    ChunkHead head;
    ChainReader2 cr2;

    std::ifstream fin(fname, std::ios::binary);
    std::vector<uint8_t> buffer(PACK_CHUNK_SIZE + CHUNK_PADDING, 0);

    hash_out = 0;

    while (fin) {
        fin.read((char*)buffer.data(), PACK_CHUNK_SIZE);
        if (!fin && fin.gcount() == 0) break;

        head.from_bytes(buffer.data());
        assert(head.body_size < PACK_CHUNK_SIZE);

        size_t buf_size = std::min(uint32_t(fin.gcount()), head.body_size);
        buf_size = std::min(buffer.size(), buf_size + CHUNK_PADDING);
        const uint8_t *ptr = buffer.data() + head.SIZE;

        uint64_t hash = 0;
        uint32_t k = 0;

        while (true) {
            PackResult pr;
            if (!is_ok(pr = cr2.start_new_chain(ptr, buf_size)))
                return false;

            hash ^= cr2.board.key();
            while (is_ok(pr = cr2.next()))
                hash ^= cr2.board.key();

            if (pr != PackResult::END_OF_CHAIN)
                return false;

            hash ^= cr2.board.do_move(cr2.move).key();

            buf_size -= cr2.tellg();
            ptr += cr2.tellg();

            if (++k >= head.n_chains)
                break;
        }

        if (ptr - buffer.data() != head.body_size + head.SIZE)
            return false;

        if (hash != head.hash)
            return false;

        hash_out ^= hash;
    }

    return true;
}

