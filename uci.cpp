#include "uci.hpp"
#include <mutex>
#include <iostream>
#include <vector>
#include <sstream>
#include "primitives/utility.hpp"

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
    void position(Listener &listener, istringstream &is) {
        Board b;

        string s, fen;
        is >> s;

        if (s == "fen") {
            while (is >> s && s != "moves")
                fen += s + ' ';
        } else if (s == "startpos") {
            fen = STARTING_FEN;
        } else {
            return;
        }

        while (is >> s) {
            Move m = move_from_str(b, s);
            if (m == MOVE_NONE)
                break;
            b = b.do_move(m);
        }

        listener.accept(cmd::Position{ b });
    }

    void go(Listener &listener, istringstream &is) {
        string token;
        cmd::Go go;
        go.max_depth = MAX_DEPTH;
        go.max_nodes = INT32_MAX;
        go.start = Clock::now();
        go.move_time = INT32_MAX;

        while (is >> token) {
            if (token == "wtime") is >> go.time_left[WHITE];
            else if (token == "btime") is >> go.time_left[BLACK];
            else if (token == "winc") is >> go.increment[WHITE];
            else if (token == "binc") is >> go.increment[BLACK];
            else if (token == "movetime") is >> go.move_time;
            else if (token == "infinite") go.infinite = true;
        }

        listener.accept(go);
    }
}

namespace UCI {

void main_loop(Listener &listener) {
    sync_cout << "id name gm_bit\n" << "id author asdf\n"
        << "uciok" << sync_endl;

    string s, cmd;
    istringstream is;
    do {
        if (!getline(cin, s))
            s = "quit";

        is.str(s);
        cmd.clear();
        is >> cmd;

        if (cmd == "isready") sync_cout << "readyok" << sync_endl;
        else if (cmd == "position") position(listener, is);
        else if (cmd == "go") go(listener, is);
        else if (cmd == "stop") listener.accept(cmd::Stop{});
        else if (cmd == "quit") listener.accept(cmd::Quit{});
    } while (s != "quit");
}

} //namespace UCI

