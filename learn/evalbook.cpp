#include <fstream>
#include <string_view>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <string>
#include <charconv>

#include "evalbook.hpp"
#include "../core/searchworker.hpp"
#include "../cli.hpp"
#include "../tt.hpp"

struct FenBatch {
    void push(std::string_view line) {
        int outcome;
        auto [ptr, ec] = std::from_chars(
            line.data(), line.data() + line.length(), outcome);

        if (ec != std::errc()) {
            sync_cout() << "you fool " << line;
            std::abort();
        }
        ++ptr;

        size_t len = line.length() - (ptr - line.data());
        std::string_view fen(ptr, len);

        memcpy(end, fen.data(), fen.length());
        end += fen.length();
        *end++ = 0;
        outcomes[n_fens] = outcome;
        lens[n_fens++] = fen.length();
    }

    bool full() const {
        return n_fens == 64;
    }

    int n_fens = 0;
    size_t lens[64];
    char buffer[8192];
    char* end = buffer;

    int8_t outcomes[64];
    int16_t scores[64];
};

int eval_book(
        const char *book_file, 
        const char *output_file,
        int depth, int time, int nb_threads)
{
    std::ifstream fin(book_file);
    if (!fin.is_open()) {
        printf("failed to open book file\n");
        return 1;
    }

    std::mutex fout_mutex;
    int fens_done = 0;
    TimePoint start = timer::now();
    std::ofstream fout(output_file);
    if (!fout.is_open()) {
        printf("failed to open output file\n");
        return 1;
    }

    Stack dummy_stack;
    std::deque<FenBatch> fen_queue;
    std::mutex queue_mutex;
    std::condition_variable cv;
    bool working = true;

    auto eval_routine = [&]() {
        SearchWorker worker;
        worker.set_silent(true);
        StateInfo si;
        Board board(&si);
        TTEntry tte;
        SearchLimits limits;
        limits.max_depth = depth;
        limits.move_time = time;
        limits.infinite = false;

        while (true) {
            std::unique_lock<std::mutex> lock(queue_mutex);
            cv.wait(lock, [&]() {
                return !working || !fen_queue.empty();
            });

            if (fen_queue.empty()) break;

            FenBatch batch = fen_queue.front();
            fen_queue.pop_front();
            lock.unlock();

            const char *ptr = batch.buffer;
            for (int i = 0; i < batch.n_fens; ++i) {
                std::string_view fen(ptr, batch.lens[i]);
                if (!board.load_fen(fen)) {
                    sync_cout() << "[ error ] invalid fen: "
                        << fen << ", aborting\n";
                    std::abort();
                }

                //do {
                    limits.start = timer::now();
                    worker.go(board, dummy_stack, limits);
                    worker.wait_for_completion();
                //} while (!g_tt.probe(board.key(), tte));
                assert(g_tt.probe(board.key(), tte));
                batch.scores[i] = tte.score16;

                ptr += fen.length() + 1;
            }

            std::lock_guard<std::mutex> fout_lock(fout_mutex);
            ptr = batch.buffer;
            for (int i = 0; i < batch.n_fens; ++i) {
                std::string_view fen(ptr, batch.lens[i]);
                fout << batch.scores[i] << ' ' 
                     << (int)batch.outcomes[i] << ' '
                     << fen << '\n';
                ptr += fen.length() + 1;
            }

            fens_done += batch.n_fens;
            TimePoint elapsed = timer::now() - start;
            int fens_per_sec = fens_done 
                / (elapsed / 1000 + 1);

            sync_cout() << fens_done << ", " << fens_per_sec
                << " fens/s\n";
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(nb_threads);
    for (int i = 0; i < nb_threads; ++i)
        threads.emplace_back(eval_routine);

    std::string line;
    FenBatch batch;
    while (std::getline(fin, line)) {
        batch.push(line);

        if (!batch.full())  continue;

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            fen_queue.push_back(batch);
        }

        cv.notify_one();
        batch.n_fens = 0;
        batch.end = batch.buffer;
    }

    if (batch.n_fens) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            fen_queue.push_back(batch);
            working = false;
        }

        cv.notify_one();
    }

    for (auto &th: threads)
        th.join();

    return 0;
}

