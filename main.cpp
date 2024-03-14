#include "core/search.hpp"
#include "zobrist.hpp"
#include "movgen/attack.hpp"
#include "tt.hpp"
#include "core/eval.hpp"
#include "cli.hpp"
#include "nnue/evaluate.hpp"

#include "core/search.hpp"

#include "pack.hpp"
#include <fstream>

using namespace std;

int main(int argc, char **argv) {
    init_zobrist();
    init_attack_tables();
    init_ps_tables();
    init_reduction_tables();
    g_tt.resize(defopts::TT_SIZE);

    if (!nnue::load_parameters(defopts::NNUE_PATH))
        printf("[WARN] failed to initialize nnue\n");

    repack_games("Release/d10v6_250k.bin", "repack.bin");
    printf("%s\n", validate_packed_games2("repack.bin") ? "valid!" : "invalid :-(");

    /* repack_games("Release/d10v6_250k.bin", "repack.bin"); */

    /* std::ifstream fin("repack.bin", std::ios::binary); */
    /* std::vector<uint8_t> buffer(PACK_CHUNK_SIZE); */

    /* fin.read((char*)buffer.data(), buffer.size()); */

    /* ChunkHead head; */
    /* head.from_bytes(buffer.data()); */

    /* ChainReader2 cr2; */
    /* PackResult pr = cr2.start_new_chain(buffer.data() + head.SIZE, buffer.size()); */


    /* return enter_cli(argc, argv); */
    return 0;
}

