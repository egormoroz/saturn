#ifndef PACK_HPP
#define PACK_HPP

#include <ostream>
#include <istream>
#include "primitives/common.hpp"
#include "bitrw.hpp"
#include "board/board.hpp"
#include "searchstack.hpp"

// size overhead is 0.05% for 1 MB
constexpr size_t PACK_CHUNK_SIZE = 1*1024*1024;
// constexpr size_t PACK_CHUNK_SIZE = 16*1024;

struct PackedBoard {
    uint64_t pc_mask;
    uint8_t pc_list[16];
};

enum GameOutcome : uint8_t {
    WHITE_WINS = 0,
    BLACK_WINS = 1,
    DRAW = 2,
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
    uint8_t buf[MAX_PLIES * 2];
    BitReader br;
    size_t is_off_start = 0, is_bytes_read = 0;

    void adjust_istream_pos(std::istream &is) const;
    void read_movescore();
};

struct PosChain {
    // a very generous upper bound
    static constexpr int MAX_PACKED_SIZE = sizeof(PackedBoard) + 2 + 4 * MAX_PLIES;

    PackedBoard start;
    uint8_t result;
    uint16_t n_moves = 0;

    struct MoveScore {
        Move move;
        int16_t score;
    } seq[MAX_PLIES];

    std::pair<uint8_t*, uint64_t> write_to_buf(uint8_t *buf, size_t buf_size) const;

    void write_to_stream(std::ostream &os) const;
    bool load_from_stream(std::istream &is);
};

struct ChunkHead {
    uint64_t hash = 0;
    uint32_t n_chains = 0;
    uint32_t body_size = 0;
    uint32_t n_pos = 0;

    static constexpr size_t SIZE = 20;

    void to_bytes(uint8_t* buf) const;
    void from_bytes(const uint8_t* buf);
};

// Stores positions into independent chunks for efficient processing
struct ChainWriter {
    ChainWriter(std::ostream &os);

    void write(const PosChain &pc);

    ~ChainWriter();

private:
    void finish_chunk(bool pad = true);

    uint8_t buf_[MAX_PLIES * 2];

    std::ostream &os_;
    // the offset to the beginning of the current chunk
    size_t chunk_start_ = 0;
    // the relative position in the current chunk
    size_t chunk_off_ = 0;

    ChunkHead head_;
};

// extra bytes so that BitReader doesn't accidentally go over 
// the buffer bounds when reading a corrupted pack.
constexpr size_t CHUNK_PADDING = 8;

struct ChainReader2 {
    // The last move isn't made
    Board board;

    uint16_t n_moves = 0;

    // The move which was made in this position to get the next one.
    // The last move is assumed to be VALID.
    Move move = MOVE_NONE;
    int16_t score;
    uint8_t result;

    // buf must have at least CHUNK_PADDING bytes of padding at the end
    PackResult start_new_chain(const uint8_t *buf, size_t buf_size);
    // Decodes the next move and score
    PackResult next();

    size_t tellg() const;

private:
    BitReader br_;
    size_t buf_size_;
    uint16_t move_idx = 0;

    // returns false on EOF
    bool read_movescore();
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

void merge_packed_games2(const char **game_fnames, int n_files, const char *fout_games);

int recover_pack(const char *fname_in, const char *fname_out, const char *hash_out);

bool repack_games(const char *fname_in, const char *fname_out);

bool validate_packed_games(const char *fname);
bool validate_packed_games(const char *fname, uint64_t hash);
bool validate_packed_games(const char *fname, const char *hashes_fname);

bool validate_packed_games2(const char *fname, uint64_t &hash_out);

// assumes the pack is valid
bool create_index(const char *fname_pack, PackIndex &pi);
bool create_index(const char *fname_pack, const char *fname_index);

#endif
