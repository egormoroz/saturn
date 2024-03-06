#ifndef PACK_HPP
#define PACK_HPP

#include <ostream>
#include <istream>
#include "primitives/common.hpp"
#include "bitrw.hpp"
#include "board/board.hpp"

struct PackedBoard {
    uint64_t pc_mask;
    uint8_t pc_list[16];
};

enum GameOutcome : uint8_t {
    WHITE_WINS = 0,
    BLACK_WINS = 1,
    DRAW = 2,
};

struct PosSeq {
    static constexpr int MAX_SEQ_LEN = 512;

    PackedBoard start;
    struct MoveScore {
        uint16_t move_idx;
        int16_t score;
    } seq[MAX_SEQ_LEN];

    uint16_t n_moves = 0;
    uint8_t result;

    void write_to_stream(std::ostream &os) const;
    bool load_from_stream(std::istream &is);
};

enum class PackResult {
    OK = 0,
    END_OF_FILE = 1,
    END_OF_CHAIN = 2,    

    // ChainReader
    UNEXPECTED_EOF,
    INVALID_HEADER,
    INVALID_BOARD,
    INVALID_MOVE,

};

constexpr inline bool is_ok(PackResult pr) { return pr == PackResult::OK; }

struct ChainReader {
    enum NextResult {
        NEXT_OK = 0,
        NEXT_END = 1,
        NEXT_ERROR = 2,
    };

    StateInfo si;
    // The last move isn't made
    Board board;

    uint16_t n_moves = 0;

    // The move which was made in this position to get the next one.
    // The last move is assumed to be VALID.
    Move move = MOVE_NONE;
    int16_t score;
    uint8_t result;

    ChainReader();
    // Reads the start position and reads a chunk of bytes for parsing and decodes the first entry.
    PackResult start_new_chain(std::istream &is);
    // Decodes the next move and score
    PackResult next(std::istream &is);

    // find first valid entry or die trying
    bool skip_invalid_data(std::istream &is);

private:
    uint16_t move_idx = 0;
    uint8_t buf[512 * 2];
    BitReader br;
    size_t is_off_start = 0, is_bytes_read = 0;

    void adjust_istream_pos(std::istream &is) const;
    void read_movescore();
};

struct PosChain {
    static constexpr int MAX_SEQ_LEN = 512;
    // a very generous upper bound
    static constexpr int MAX_PACKED_SIZE = sizeof(PackedBoard) + 2 + 4 * MAX_SEQ_LEN;

    PackedBoard start;
    uint8_t result;
    uint16_t n_moves = 0;

    struct MoveScore {
        Move move;
        int16_t score;
    } seq[MAX_SEQ_LEN];

    void write_to_stream(std::ostream &os) const;
    bool load_from_stream(std::istream &is);
};

struct PackIndex {
    static constexpr int MAX_BLOCKS = 32;
    struct Block {
        size_t off_begin, off_end;
        size_t n_pos;
        // Maybe extra info here...
    } blocks[MAX_BLOCKS];
    size_t n_blocks = 0;

    void write_to_stream(std::ostream &os) const;
    bool load_from_stream(std::istream &is);
};

PackedBoard pack_board(const Board &b);
[[nodiscard]] bool unpack_board(const PackedBoard &pb, Board &b);
uint64_t packed_hash(const PackedBoard &pb);

void merge_packed_games(const char **game_fnames, const char **hash_fnames,
        int n_files, const char *fout_games, const char *fout_hash);

void repack_games(const char *fname_in, const char *fname_out);
int recover_pack(const char *fname_in, const char *fname_out, const char *hash_out);

bool validate_packed_games(const char *fname);
bool validate_packed_games(const char *fname, uint64_t hash);
bool validate_packed_games(const char *fname, const char *hashes_fname);

// assumes the pack is valid
bool create_index(const char *fname_pack, const char *fname_index);

#endif
