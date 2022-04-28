#include "evalbook.hpp"

#include <cstdint>
#include <fstream>
#include <string>
#include <iostream>
#include <iomanip>
#include <vector>
#include <deque>
#include <array>
#include <cstring>

#include "../board/board.hpp"
#include "../core/searchworker.hpp"
#include "../tt.hpp"
#include "../movgen/generate.hpp"
#include "../cli.hpp"

struct Position {
    char fen[128];
    int16_t score;
    int8_t outcome;
    bool filtered_out;
};

using std::string;
using std::endl;
using std::cerr;
using std::quoted;

bool filter_out_pos(const Board &b, const TTEntry &tte) {
    Move m = Move(tte.move16);
    if (abs(tte.score16) >= 6666
        || b.is_material_draw()
        || !b.is_valid_move(m)
        || !b.is_quiet(m))
        return true;
    return false;
}

constexpr int BATCH_SIZE = 64;
struct Batch {
    std::array<Position, BATCH_SIZE> pos;
    int n = 0;
};

int eval_book(
        const char *book_file, 
        const char *output_file,
        int depth, int time, int nb_threads)
{
    std::ifstream fin(book_file);
    if (!fin.is_open()) {
        cerr << "failed to open book file " 
            << quoted(book_file) << endl;
        return 1;
    }

    std::mutex fout_mutex;
    std::ofstream fout(output_file);
    if (!fout.is_open()) {
        cerr << "failed to create output book file " 
            << quoted(output_file) << endl;
        return 1;
    }

    TimePoint start = timer::now();
    int64_t fens_done = 0;
    bool done = false;
    std::condition_variable batch_ready_cv;
    std::mutex q_mutex;
    std::deque<Batch> batches;

    auto eval_routine = [&]() {
        StateInfo si;
        Board board(&si);
        SearchWorker search;
        search.set_silent(true);
        SearchLimits limits;
        limits.max_depth = depth;
        limits.move_time = time;

        ExtMove move_buf[MAX_MOVES];

        TTEntry tte;
        while (true) {
            std::unique_lock<std::mutex> lock(q_mutex);
            batch_ready_cv.wait(lock, [&]() {
                return done || batches.size();
            });

            if (batches.empty()) break;
            Batch b = batches.front();
            batches.pop_front();
            lock.unlock();

            for (int i = 0; i < b.n; ++i) {
                Position &pos = b.pos[i];
                if (!board.load_fen(pos.fen)) {
                    sync_cout() << "invalid fen \""
                        << pos.fen << "\", skipping...\n";
                    pos.filtered_out = true;
                    continue;
                }

                //no legal moves
                if (generate<LEGAL>(board, move_buf) == move_buf) {
                    pos.filtered_out = true;
                    continue;
                }

                limits.start = timer::now();
                search.go(board, limits);
                search.wait_for_completion();

                if (!g_tt.probe(board.key(), tte)) {
                    sync_cout() << "failed to probe result"
                        << " from tt, skipping\n";
                    pos.filtered_out = true;
                    continue;
                }

                pos.score = tte.score16;
                pos.filtered_out = filter_out_pos(board, tte);
            }

            std::lock_guard<std::mutex> fout_lock(fout_mutex);
            for (int i = 0; i < b.n; ++i) {
                const Position &pos = b.pos[i];
                if (pos.filtered_out)  continue;
                fout << pos.score << ' ' 
                     << int(pos.outcome) << ' '
                     << pos.fen << '\n';
                ++fens_done;
            }

            int64_t fens_per_sec = fens_done * 1000 
                / (timer::now() - start);
            sync_cout() << fens_done << ", " << fens_per_sec
                << " fens/s" << '\n';
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < nb_threads; ++i)
        threads.emplace_back(eval_routine);

    std::string line;
    line.reserve(256);

    Batch b;

    int outcome;
    while (fin >> outcome) {
        //consume whitespace
        if (!std::getline(fin, line)) {
            cerr << "unexpected eof" << endl;
            return 1;
        }
        std::string_view fen(line);
        fen = fen.substr(1);

        Position &pos = b.pos[b.n++];
        pos.outcome = static_cast<int8_t>(outcome);

        memcpy(pos.fen, fen.data(), fen.length());
        pos.fen[fen.length()] = 0;

        if (b.n >= BATCH_SIZE) {
            std::unique_lock<std::mutex> q_lock(q_mutex);
            while (batches.size() >= 64) {
                q_lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                q_lock.lock();
            }
            batches.push_back(b);

            q_lock.unlock();
            batch_ready_cv.notify_one();
            b.n = 0;
        }
    }

    {
        std::lock_guard<std::mutex> lock(q_mutex);
        if (b.n)
            batches.push_back(b);
        done = true;
    }

    batch_ready_cv.notify_all();
    for (auto &th: threads)
        th.join();

    return 0;
}

