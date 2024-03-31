#ifndef SELFPLAY_HPP
#define SELFPLAY_HPP

// outputs two files: <out_name>.bin and <out_name>.hash
void selfplay(const char *out_name, int num_pos, int nodes, 
        int n_pvs, int max_ld_moves, int n_threads);

#endif
