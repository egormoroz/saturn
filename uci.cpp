#include <algorithm>
#include <sstream>
#include <cctype>

#include "uci.hpp"
#include "primitives/utility.hpp"
#include "tt.hpp"
#include "mininnue/nnue.hpp"
#include "scout.hpp"


#include "perft.hpp"

#include "zobrist.hpp"

UCIContext::UCIContext()
    : board_(&si_)
{
}

void UCIContext::enter_loop() {
    board_ = Board::start_pos(&si_);
    mini::refresh_accumulator(board_, si_.acc, WHITE);
    mini::refresh_accumulator(board_, si_.acc, BLACK);
    st_.reset();

    std::string s, cmd;
    std::istringstream is;

    do {
        if (!std::getline(std::cin, s))
            s = "quit";

        is.str(s);
        is.clear();
        cmd.clear();
        is >> cmd;

        if (cmd == "isready") sync_cout() << "readyok\n";
        else if (cmd == "uci") print_info();
        else if (cmd == "position") parse_position(is);
        else if (cmd == "go") parse_go(is);
        else if (cmd == "setoption") parse_setopt(is);
        else if (cmd == "stop") search_.stop();
        else if (cmd == "ponderhit") search_.stop_pondering();
        else if (cmd == "d") sync_cout() << board_;
        else if (cmd == "quit") break;

    } while (s != "quit");
}

void UCIContext::parse_position(std::istream &is) {
    std::string s, fen;
    is >> s;
    st_.reset();

    if (s == "fen") {
        while (is >> s && s != "moves")
            fen += s + ' ';
        bool result = board_.load_fen(fen);
        assert(result);
    } else if (s == "startpos") {
        board_ = Board::start_pos(&si_);
        is >> s; //consume "moves"
    } else {
        return;
    }

    if (s != "moves")
        return;

    while (is >> s) {
        Move m = move_from_str(board_, s);
        if (m == MOVE_NONE)
            break;
        st_.push(board_.key(), m);
        board_ = board_.do_move(m, &si_);
    }
    st_.set_start(st_.total_height());
    si_.reset();
}

void UCIContext::parse_go(std::istream &is) {
    // TODO: make max book depth dynamic
    if (book_loaded_ && st_.total_height() <= 16) {
        uint64_t key = board_.key();
        Square ep = board_.en_passant();
        if (is_ok(ep))
            key ^= ZOBRIST.enpassant[file_of(ep)];

        if (Move m = book_.probe(key); is_ok(m) && board_.is_valid_move(m)) {
            sync_cout() << "bestmove " << m << '\n';
            return;
        }
    }

    std::string token;

    bool ponder = false;
    SearchLimits limits;
    limits.type = limits.TIME;
    limits.start = timer::now();

    while (is >> token) {
        if (token == "wtime") is >> limits.time[WHITE];
        else if (token == "btime") is >> limits.time[BLACK];
        else if (token == "winc") is >> limits.inc[WHITE];
        else if (token == "binc") is >> limits.inc[BLACK];
        else if (token == "movetime") is >> limits.move_time;

        else if (token == "infinite") limits.type = limits.UNLIMITED;

        else if (token == "ponder") { ponder = true; limits.type = limits.UNLIMITED; }
        else if (token == "depth") { is >> limits.depth; limits.type = limits.DEPTH; }
        else if (token == "nodes") { is >> limits.nodes; limits.type = limits.NODES; }
        else if (token == "perft") { parse_go_perft(is); return; }
    }

    search_.go(board_, limits, st_.total_height() ? &st_ : nullptr, ponder, multipv_);
}

void UCIContext::parse_go_perft(std::istream &is) {
    int depth = 1;
    if ((is >> depth) && depth < 1)
        return;

    auto start = timer::now();
    uint64_t nodes = perft(board_, depth);
    auto delta = timer::now() - start;

    // (nodes / (delta_ms / 1000) / 1'000'000
    uint64_t mnps = nodes / (delta * 1'000);

    sync_cout() << nodes << " nodes @ " << mnps << " mn/s\n";
}

void UCIContext::parse_setopt(std::istream &is) {
    std::string name, t;
    is >> t >> name;

    if (t != "name") return;

    std::transform(name.begin(), name.end(), name.begin(),
        [](char ch) { return std::tolower(ch); });

    if (name == "hash") {
        if (is >> t; t != "value") return;

        int value = -1;
        if (is >> value && value > 0) {
            search_.stop();
            search_.wait_for_completion();
            g_tt.resize(value);
        }
    } else if (name == "clear") {
        if (is >> t; t != "hash") return;

        g_tt.clear();
    } else if (name == "multipv") {
        if (is >> t; t != "value") return;

        int value = -1;
        if (is >> value && value > 0)
            multipv_ = value;
    } else if (name == "evalfile") {
        if (is >> t; t != "value") return;
        if (!std::getline(is, t)) return;

        const char* path = t.c_str();
        while (*path && std::isspace(*path))
            ++path;

        if (mini::load_parameters(path)) {
            sync_cout() << "info string NNUE initialized from file " << path << "\n";
            mini::refresh_accumulator(board_, si_.acc, WHITE);
            mini::refresh_accumulator(board_, si_.acc, BLACK);
        } else {
            sync_cout() << "info string Failed to initialize NNUE from file " << path << "\n";
        }
    } else if (name == "aspdelta") {
        if (is >> t; t != "value") return;

        int value = -1;
        if (is >> value && value > 0)
            params::asp_init_delta = value;
    } else if (name == "aspmindepth") {
        if (is >> t; t != "value") return;

        int value = -1;
        if (is >> value && value > 0)
            params::asp_min_depth = value;
    } else if (name == "lmrcoeff") {
        if (is >> t; t != "value")
            return;
        if (float value; is >> value) {
            params::lmr_coeff = value;
            update_reduction_tables(value);
        }
    } else if (name == "moveoverhead") {
        if (is >> t; t != "value") return;

        int value = -1;
        if (is >> value && value > 0)
            params::move_overhead = value;
    } else if (name == "bookfile") {
        if (is >> t; t != "value") return;
        if (!std::getline(is, t) || t.empty()) return;


        const char* path = t.c_str();
        while (*path && std::isspace(*path))
            ++path;

        if (t.find(".bin") != std::string::npos)
            book_loaded_ = book_.load_from_bin(path);
        else
            book_loaded_ = book_.load_from_fens(path);

        sync_cout() << "info string book is " << (book_loaded_ ? "" : "not ") << "loaded\n";
    }
}

void UCIContext::print_info() {
    namespace d = params::defaults;

    sync_cout() << "id name saturn 1.2\n" << "id author egormoroz\n"
        <<  "option name Ponder type check default false\n"
        <<  "option name clear hash type button\n"
        <<  "option name multipv type spin default 1 min 1 max 256\n"
        <<  "option name evalfile type string default "
            << d::nnue_weights_path << '\n'

        << "option name Hash type spin default " 
            << d::tt_size << " min 16 max 4096\n"

        <<  "option name aspdelta type spin default "
            << d::asp_init_delta << " min 1 max 100\n"

        <<  "option name aspmindepth type spin default "
            << d::asp_init_delta << " min 1 max 100\n"

        <<  "option name MoveOverhead type spin default "
            << d::move_overhead << " min 0 max 1000\n"

        <<  "option name lmrcoeff type string default "
            << d::lmr_coeff << '\n'

        <<  "option name bookfile type string default <DISABLED>\n";

    sync_cout() << "uciok\n";
}

