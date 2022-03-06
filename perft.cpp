#include "perft.hpp"
#include "movgen/generate.hpp"
#include "board/board.hpp"
#include <vector>
#include <thread>
#include "movepicker.hpp"

namespace {

struct PerftResult {
    std::string_view fen;
    int depth;
    uint64_t nodes;
};

constexpr PerftResult PERFT_RESULTS[] = {
    { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 6, 119'060'324 },
    { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ", 5, 193'690'690 },
    { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ", 7, 178'633'661 },
    { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 5, 15'833'292 },
    { "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8  ", 5, 89'941'194 },
    { "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 ", 5, 164'075'551 },
};

constexpr int N = static_cast<int>(std::size(PERFT_RESULTS));

} //namespace

uint64_t perft(const Board &b, int depth) {
    ExtMove begin[MAX_MOVES], *end;
    end = generate<CAPTURES>(b, begin);
    end = generate<QUIET>(b, end);

    if (depth == 0)
        return 1;
    if (depth == 1)
        return end - begin;

    uint64_t n = 0;
    for (auto it = begin; it != end; ++it) {
        n += perft(b.do_move(*it), depth - 1);
    }

    return n;
}

int perft_test_positions() {
    uint64_t results[N]{};
    std::vector<std::thread> threads;
    threads.reserve(N);

    for (int i = 0; i < N; ++i) {
        threads.emplace_back([&results, i]() {
            PerftResult pr = PERFT_RESULTS[i];
            Board b;
            if (!b.load_fen(pr.fen))
                return;
            results[i] = perft(b, pr.depth);
        });
    }

    for (auto &t: threads)
        t.join();

    for (int i = 0; i < N; ++i) {
        if (PERFT_RESULTS[i].nodes != results[i])
            return i + 1;
    }

    return 0;
}

