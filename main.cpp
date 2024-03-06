#include "core/search.hpp"
#include "zobrist.hpp"
#include "movgen/attack.hpp"
#include "tt.hpp"
#include "core/eval.hpp"
#include "cli.hpp"
#include "nnue/evaluate.hpp"

#include "core/search.hpp"

using namespace std;

int main(int argc, char **argv) {
    init_zobrist();
    init_attack_tables();
    init_ps_tables();
    init_reduction_tables();
    g_tt.resize(defopts::TT_SIZE);

#ifdef NONNUE
    printf("NNUE is disabled, using regular eval\n");
#else
    if (!nnue::load_parameters(defopts::NNUE_PATH))
        printf("[WARN] failed to initialize nnue\n");
#endif

    return enter_cli(argc, argv);
}

