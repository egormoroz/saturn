#include <iostream>
#include <string>
#include <vector>
#include "primitives/utility.hpp"
#include "zobrist.hpp"
#include "movgen/attack.hpp"
#include "board/board.hpp"
#include "tt.hpp"
#include "uci.hpp"
#include "perft.hpp"
#include "core/eval.hpp"
#include "core/engine.hpp"
#include "tree.hpp"
#include <fstream>
#include <sstream>

using namespace std;
constexpr std::string_view STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";


#ifdef TRACE
void walk_tree();
#endif

int main() {
    init_zobrist();
    init_attack_tables();
    init_ps_tables();
    SearchContext::init_tables();
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
    Engine eng(1);
    
    while (true) {
        Board &b = history.back();
        cin >> token;
        Move m;
        if (token == "d") {
            cout << b << endl;
        } else if (token == "uci") {
            UCI::main_loop(eng);
            break;
        } else if (token == "u") {
            if (history.size() > 1)
                history.pop_back();
        } else if (token == "q") {
            break;
        } else if (token == "s") {
            int depth, time;
            cin >> depth >> time;
            eng.start(b, nullptr, depth, time);
            eng.wait_for_completion();
        }  else if (token == "fen") {
            getline(cin, token);
            history.resize(1);
            if (!history.back().load_fen(token)) {
                cout << "invalid fen\n";
                bool result = history.back().load_fen(STARTING_FEN);
                assert(result);
                history.back().validate();
            }
        } 
#ifdef TRACE
        else if (token == "w") {
            walk_tree();
        } else if (token == "j") {
            ofstream fout("tree.json");
            g_tree.json(fout);
        }
#endif
        else if((m = move_from_str(b, token)) != MOVE_NONE) {
            if (b.piece_on(to_sq(m)) != NO_PIECE) {
                cout << "Ok capture: " << boolalpha 
                    << b.ok_capture(m) << "\n";
            }
            history.push_back(b.do_move(m));
        } else {
            cout << "unknown command / illegal move" << endl;
        }
    }

    return 0;
}

#ifdef TRACE

template<bool root>
void print_tree(vector<size_t> &nodes, size_t parent, int depth) {
    if (!root) {
        nodes.push_back(parent);
        cout << g_tree.nodes[parent] << '\n';
    }
    if (!depth)
        return;

    size_t child = root ? 0 : g_tree.first_child(parent);
    while (child != Tree::npos) {
        print_tree<false>(nodes, child, depth - 1);
        child = g_tree.next_child(child);
    }
}

void walk_tree() {
    if (!g_tree.size())
        return;

    int depth = 1;
    string token;
    size_t parent = Tree::npos;
    vector<size_t> nodes;
    ostringstream ss;

    while (true) {
        nodes.clear();
        if (parent == Tree::npos)
            print_tree<true>(nodes, parent, depth);
        else
            print_tree<false>(nodes, parent, depth);

        cout << "walker > ";
        cin >> token;

        if (token == "q")
            break;
        else if (token == "setd")
            cin >> depth;
        else if (token == "d")
            cout << depth << '\n';
        else if (token == "sel") {
            cin >> token;
            for (size_t i: nodes) {
                ss.str("");
                ss.clear();
                ss << g_tree.nodes[i].prev_move;
                if (ss.str() == token) {
                    parent = i;
                    break;
                }
            }

        } else if (token == "root") {
            parent = Tree::npos;
        } else if (token == "up") {
            parent = g_tree.parent(parent);
        }
    }
}

#endif
