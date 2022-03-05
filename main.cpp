#include <iostream>
#include "perft.hpp"
#include "zobrist.hpp"
#include "movgen/attack.hpp"
#include "board/board.hpp"
#include "movgen/generate.hpp"
#include "primitives/utility.hpp"
#include <chrono>

using namespace std;

constexpr std::string_view STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

template<typename F>
long long time_it(F &&f) {
    using Clock = chrono::steady_clock;
    auto start = Clock::now();
    f();
    return chrono::duration_cast<chrono::milliseconds>(Clock::now() - start).count();
}

int main() {
    init_zobrist();
    init_attack_tables();

    string_view fen = "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10";
    fen = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
    fen = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ";
    fen = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ";
    fen = "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1";

    Board b;
    b.load_fen(fen);

    uint64_t n = 0;
    auto millis = time_it([&](){
        n = perft(b, 4);
    });
    cout << "total nodes: " << n
        << "\nnps: " << (n / millis / 1000) << "mil" << endl;

    /* int result = perft_test_positions(); */
    /* if (result != 0) { */
    /*     cout << "failed: " << result; */
    /*     return 1; */
    /* } */
    /* cout << "perft tests passed!" << endl; */

    return 0;
}

