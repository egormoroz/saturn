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

    Board b;
    b.load_fen(STARTING_FEN);

    uint64_t n = 0;
    auto millis = time_it([&](){
        n = perft(b, 7);
    });
    cout << "total nodes: " << n
        << "\nnps: " << (n / millis / 1000) << "mil" << endl;

    /* int result = perft_test_positions(); */
    /* if (result != 0) { */
    /*     cout << "failed: " << result; */
    /*     return 1; */
    /* } */

    return 0;
}

