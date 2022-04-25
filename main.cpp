#include "zobrist.hpp"
#include "movgen/attack.hpp"
#include "tt.hpp"
#include "core/eval.hpp"
#include "cli.hpp"
#include "nnue/evaluate.hpp"

using namespace std;

int main(int argc, char **argv) {
    init_zobrist();
    init_attack_tables();
    init_ps_tables();
    init_reduction_tables();
    g_tt.resize(128);

    if (!nnue::load_parameters("64_32_32.nnue")) {
        printf("failed to initialize nnue, aborting\n");
        return 1;
    }

    return enter_cli(argc, argv);
}

