#include "uci.hpp"

#include "selfplay.hpp"
#include "pack.hpp"
#include "tt.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>

static void run_bench(int argc, char **argv);

int main(int argc, char **argv) {
    if (argc == 1) {
        UCIContext().enter_loop();
        return 0;
    }

    if (argc < 2) {
        (void)(argc);
        (void)(argv);

        UCIContext uci;
        uci.enter_loop();

        return 0;
    }

    if (!strcmp(argv[1], "selfplay")) {
        if (argc != 9) {
            printf("usage: selfplay <out_name> <num_pos> <nodes> "
                   "<n_psv> <max_ld_moves> <n_threads>\n");
            return 1;
        }
        
        const char *out_name = argv[2];
        int num_pos = atol(argv[3]);
        int nodes = atol(argv[4]);
        int n_pv = atol(argv[5]);
        int max_ld_moves = atol(argv[6]);
        int n_threads = atol(argv[7]);

        selfplay(out_name, num_pos, nodes, n_pv, max_ld_moves, n_threads);

        return 0;
    } else if (!strcmp(argv[1], "packval")) {
        if (argc != 3) {
            printf("usage: packval <pack_fin>\n");
            return 1;
        }

        uint64_t hash = 0;
        bool is_valid = validate_packed_games2(argv[2], hash);
        if (is_valid)
            printf("valid! hash %llu\n", (unsigned long long)hash);
        else
            printf("invalid :-(\n");

        return 0;
    } else if (!strcmp(argv[1], "packstats")) {
        if (argc != 3) {
            printf("usage: packstats <pack_fin>\n");
            return 1;
        }

        std::ifstream fin(argv[2], std::ios::binary);
        if (!fin) {
            printf("could not open file %s\n", argv[2]);
            return 1;
        }

        ChunkHead head;
        uint8_t buffer[head.SIZE];

        unsigned long long cum_hash = 0, n_chains = 0, n_pos = 0;

        while (true) {
            if (!fin.read((char*)buffer, sizeof(buffer)))
                break;

            head.from_bytes(buffer);
            cum_hash ^= head.hash;
            n_chains += head.n_chains;
            n_pos += head.n_pos;

            fin.ignore(PACK_CHUNK_SIZE - head.SIZE);
        }

        printf("Hash %llu\nNumber of chains %llu\nNumber of positions %llu\n",
                cum_hash, n_chains, n_pos);

        return 0;
    } else if (!strcmp(argv[1], "packmerge")) {
        if (argc < 5) {
            printf("usage: packmerge <fout_bin> <n_files> <fbin1> <fbin2>...\n");
            return 1;
        }

        merge_packed_games2((const char**)&argv[4], atol(argv[3]), argv[2]);
        return 0;
    } else if (!strcmp(argv[1], "cvtbook")) {
        if (argc != 4) {
            printf("usage: cvtbook <txtbook_iN> <binbook_out>\n");
            return 1;
        }

        Book b;
        if (!b.load_from_fens(argv[2])) {
            printf("failed to load book from %s\n", argv[2]);
            return 1;
        }

        if (!b.save_to_bin(argv[3])) {
            printf("failed to save book to %s\n", argv[2]);
            return 1;
        }

        printf("binarized book saved to %s\n", argv[3]);

        return 0;
    } else if (!strcmp(argv[1], "bench")) {
        run_bench(argc, argv);
        return 0;
    }

    printf("invalid command line arguments\n");
    return 1;
}

static void run_bench(int argc, char **argv) {
    (void)(argc);
    (void)(argv);

    static const char *bench_fens[] = {
        #include "bench.csv"
    };

    constexpr int N_FENS = std::size(bench_fens);

    int nodes[N_FENS];
    int times[N_FENS];
    int nps[N_FENS];
    Move best_moves[N_FENS];
    int scores[N_FENS];

    std::unique_ptr<Search> search(new Search);
    search->set_silent(true);

    SearchLimits limits;
    limits.depth = 13;
    limits.type = limits.DEPTH;

    Board b;

    for (int i = 0; i < N_FENS; ++i) {
        b.load_fen(bench_fens[i]);
        
        limits.start = timer::now();

        search->setup(b, limits);
        search->iterative_deepening();

        auto stats = search->get_stats();

        nodes[i] = int(stats.nodes);
        times[i] = int(timer::now() - limits.start);
        nps[i] = 1000 * nodes[i] / std::max(1, times[i]);
        best_moves[i] = search->get_pv_start(0).move;
        scores[i] = search->get_pv_start(0).score;

        g_tt.clear();
    }

    for (int i = 0; i < N_FENS; ++i) {
        std::cout << "[ # " << std::setw(2) << i << " ]"
            << " bestmove " << std::setw(0) << best_moves[i]
            << " score "    << std::setw(5) << scores[i]
            << " nodes "    << std::setw(8) << nodes[i]
            << " nps "      << std::setw(8) << nps[i]
            << '\n';
    }

    int64_t total_nodes = std::accumulate(std::begin(nodes), std::end(nodes), 0);
    int avg_nps = 1000 * total_nodes / std::accumulate(std::begin(times), std::end(times), 0);

    std::cout << "\noverall " 
        << std::setw(9) << total_nodes << " nodes"
        << std::setw(9) << avg_nps << " nps" << std::endl;
}
