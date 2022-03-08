#include "uci.hpp"
#include <mutex>
#include <iostream>
#include <vector>
#include <sstream>
#include "primitives/utility.hpp"
#include "core/engine.hpp"

using std::cin, std::string, std::istringstream, std::getline;
constexpr std::string_view STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

std::ostream& operator<<(std::ostream& os, SyncCout sc) {
    static std::mutex mtx;
    if (sc == IO_LOCK)
        mtx.lock();
    if (sc == IO_UNLOCK)
        mtx.unlock();

    return os;
}

using namespace UCI;

namespace {
    void position(Board &b, History &hist, istringstream &is) {
        string s, fen;
        is >> s;
        hist.ply = 0;

        if (s == "fen") {
            while (is >> s && s != "moves")
                fen += s + ' ';
        } else if (s == "startpos") {
            fen = STARTING_FEN;
            is >> s; //consume "moves"
        } else {
            return;
        }

        if (!b.load_fen(fen))
            return;

        while (is >> s) {
            Move m = move_from_str(b, s);
            if (m == MOVE_NONE)
                break;
            hist.push(b.key(), m);
            b = b.do_move(m);
        }
    }

    void go(Engine &e, Board &b, History &hist, istringstream &is) {
        string token;
        int time_left[2]{}, increment[2]{};
        int max_depth = MAX_DEPTH;
        /* int max_nodes = INT32_MAX; */
        int move_time = 0;
        bool infinite = false;

        while (is >> token) {
            if (token == "wtime") is >> time_left[WHITE];
            else if (token == "btime") is >> time_left[BLACK];
            else if (token == "winc") is >> increment[WHITE];
            else if (token == "binc") is >> increment[BLACK];
            else if (token == "movetime") is >> move_time;
            else if (token == "infinite") infinite = true;
            else if (token == "depth") is >> max_depth;
        }

        Color us = b.side_to_move();
        int millis_left = time_left[us], inc = increment[us];
        int max_millis = millis_left / 55 + inc - 200;
        max_millis = std::max(max_millis, move_time);
        if (infinite)
            max_millis = -1;

        e.start(b, &hist, max_depth, max_millis);
    }
}

namespace UCI {

void main_loop(Engine &e) {
    sync_cout << "id name gm_bit[IID/RFP/LMR]\n" << "id author asdf\n"
        << "uciok" << sync_endl;

    Board b;
    History hist;

    string s, cmd;
    istringstream is;
    do {
        if (!getline(cin, s))
            s = "quit";

        is.str(s);
        is.clear();
        is >> cmd;

        if (cmd == "isready") sync_cout << "readyok" << sync_endl;
        else if (cmd == "position") position(b, hist, is);
        else if (cmd == "go") go(e, b, hist, is);
        else if (cmd == "stop") e.stop_search();
        else if (cmd == "d") sync_cout << b << sync_endl;
        else if (cmd == "quit") break;
    } while (s != "quit");
}

} //namespace UCI

