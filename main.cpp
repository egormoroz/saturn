#include "core/search.hpp"
#include "zobrist.hpp"
#include "movgen/attack.hpp"
#include "tt.hpp"
#include "core/eval.hpp"
#include "cli.hpp"
#include "nnue/evaluate.hpp"

int main(int argc, char **argv) {
    init_zobrist();
    init_attack_tables();
    init_ps_tables();
    init_reduction_tables();
    g_tt.resize(defopts::TT_SIZE);

    if (!nnue::load_parameters(defopts::NNUE_PATH))
        printf("[WARN] failed to initialize nnue\n");

    return enter_cli(argc, argv);
}

