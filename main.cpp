#include <iostream>
#include <string>
#include <vector>
#include "primitives/utility.hpp"
#include "zobrist.hpp"
#include "movgen/attack.hpp"
#include "board/board.hpp"
#include "tt.hpp"
#include "uci.hpp"
#include "core/search.hpp"
#include "perft.hpp"
#include "movgen/generate.hpp"
#include "core/eval.hpp"
#include "core/engine.hpp"

using namespace std;
constexpr std::string_view STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

struct MyListener : public SearchListener {
    virtual void accept(int, const SearchReport &report) final {
        sync_cout << report << sync_endl;
    }

    virtual void on_search_finished(int, Move best_move) final {
        sync_cout << "bestmove " << best_move << sync_endl;
    }
};

int main() {
    init_zobrist();
    init_attack_tables();
    init_ps_tables();
    g_tt.init(128);

    std::vector<Board> history;
    history.reserve(MAX_MOVES);
    history.resize(1);
    if (!history.back().load_fen(STARTING_FEN)) {
        cout << "failed to load starting fen" << endl;
        return 1;
    }
    history.back().validate();

    string token;
    MyListener listener;
    /* SearchContext search; */
    /* search.add_listener(&listener); */

    Engine eng;
    
    while (true) {
        Board &b = history.back();
        cout << b << endl;

        cin >> token;
        Move m;
        if (token == "uci") {
            /* UCI::main_loop(search); */
            break;
        } else if (token == "u") {
            if (history.size() > 1)
                history.pop_back();
        } else if (token == "q") {
            break;
        } else if (token == "s") {
            eng.start(b, MAX_DEPTH, -1);
            eng.wait_for_completion();
            int n = eng.total_nodes() / 10'000;
            cout << "nps: " << n << "k" << endl;
        } else if((m = move_from_str(b, token)) != MOVE_NONE) {
            if (b.piece_on(to_sq(m)) != NO_PIECE) {
                cout << "Ok capture: " << boolalpha 
                    << b.ok_capture(m) << "\n";
            }
            history.push_back(b.do_move(m));
        } else if (token == "fen") {
            getline(cin, token);
            history.resize(1);
            if (!history.back().load_fen(token)) {
                cout << "invalid fen\n";
                bool result = history.back().load_fen(STARTING_FEN);
                assert(result);
            }
        } else {
            cout << "unknown command / illegal move" << endl;
        }
    }

    return 0;
}

