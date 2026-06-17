#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <omp.h>
#include "maze.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "how to use: %s width height [seed] [p]\n", argv[0]);
        return 1;
    }
    width = atoi(argv[1]);          // args: width height [seed] [p]
    int height = atoi(argv[2]);
    uint64_t seed = argc > 3 ? strtoull(argv[3], NULL, 10) : 1234;
    int do_print = argc > 4 && argv[4][0] == 'p';

    int cells = width * height;
    wall_right = malloc(cells);
    wall_down = malloc(cells);
    visited = calloc(cells, 1);
    memset(wall_right, 1, cells);   // every wall closed to start
    memset(wall_down, 1, cells);

    int thread_count = omp_get_max_threads();
    if (thread_count > height) thread_count = height;   // at most one strip per row

    double start_time = omp_get_wtime();

    #pragma omp parallel num_threads(thread_count)
    {                               // each thread builds its own strip, no sharing
        int tid = omp_get_thread_num();
        int rows_each = height / thread_count, leftover = height % thread_count;
        int row_start = tid * rows_each + (tid < leftover ? tid : leftover);
        int row_end = row_start + rows_each + (tid < leftover ? 1 : 0);

        uint64_t thread_seed = seed + (uint64_t)tid * 2654435761u + 1;   // distinct per thread
        if (thread_seed == 0) thread_seed = 1;
        carve(row_start, row_end, thread_seed);
    }

    uint64_t stitch_seed = seed * 2862933555777941757ull + 3037000493ull;
    int rows_each = height / thread_count, leftover = height % thread_count;
    for (int strip = 0; strip < thread_count - 1; strip++) {   // join strips: one opening per border
        int row_start = strip * rows_each + (strip < leftover ? strip : leftover);
        int row_end = row_start + rows_each + (strip < leftover ? 1 : 0);
        int boundary_row = row_end - 1;
        int opening_col = next_rand(&stitch_seed) % width;
        wall_down[boundary_row * width + opening_col] = 0;
    }

    double end_time = omp_get_wtime();

    fprintf(stderr, "%d %d %d %.6f\n", width, height, thread_count, end_time - start_time);

    if (do_print) print_maze(height);

    free(wall_right);
    free(wall_down);
    free(visited);
    return 0;
}
