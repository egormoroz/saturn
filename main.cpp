#include <iostream>
#include <vector>
#include "primitives/utility.hpp"
#include "zobrist.hpp"
#include "movgen/attack.hpp"
#include "board/board.hpp"
#include "tt.hpp"
#include "uci.hpp"
#include "core/search.hpp"

using namespace std;
constexpr std::string_view STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

int main() {
    init_zobrist();
    init_attack_tables();
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
    SearchContext search;
    
    while (true) {
        Board &b = history.back();
        cout << b << endl;

        cin >> token;
        Move m;
        if (token == "uci") {
            UCI::main_loop(search);
            break;
        } else if (token == "u") {
            if (history.size() > 1)
                history.pop_back();
        } else if (token == "q") {
            break;
        } else if (token == "s") {
            search.set_board(b);
            search.run(MAX_DEPTH, 5000, false);
            search.wait_for_search();
        } else if((m = move_from_str(b, token)) != MOVE_NONE) {
            history.push_back(b.do_move(m));
        } else {
            cout << "unknown command / illegal move" << endl;
        }
    }

    return 0;
}

