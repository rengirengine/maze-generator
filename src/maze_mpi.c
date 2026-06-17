#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <mpi.h>
#include "maze.h"

static uint64_t hash64(uint64_t value) {   // splitmix64, agreed border column
    value += 0x9E3779B97F4A7C15ull;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
    return value ^ (value >> 31);
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int rank, nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    if (argc < 3) {
        if (rank == 0) fprintf(stderr, "how to use: %s width height [seed] [p]\n", argv[0]);
        MPI_Finalize();
        return 1;
    }
    width = atoi(argv[1]);          // args: width height [seed] [p]
    int height = atoi(argv[2]);
    uint64_t seed = argc > 3 ? strtoull(argv[3], NULL, 10) : 1234;
    int do_print = argc > 4 && argv[4][0] == 'p';

    int rows_each = height / nprocs, leftover = height % nprocs;   // split rows over ranks
    int row_start = rank * rows_each + (rank < leftover ? rank : leftover);
    int row_end = row_start + rows_each + (rank < leftover ? 1 : 0);
    int my_rows = row_end - row_start;

    wall_right = malloc(my_rows * width);
    wall_down = malloc(my_rows * width);
    visited = calloc((size_t)my_rows * width, 1);
    memset(wall_right, 1, my_rows * width);   // every wall closed to start
    memset(wall_down, 1, my_rows * width);

    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();

    if (my_rows > 0) carve(0, my_rows, seed + (uint64_t)rank * 2654435761u + 1);   // local strip rows [0, my_rows)
    if (my_rows > 0 && row_end < height) {   // upper rank opens one border wall, no message
        int opening_col = hash64(seed + rank) % width;
        wall_down[(my_rows - 1) * width + opening_col] = 0;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double end_time = MPI_Wtime();

    double elapsed = end_time - start_time, slowest;
    MPI_Reduce(&elapsed, &slowest, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);   // parallel time = slowest rank
    if (rank == 0) fprintf(stderr, "%d %d %d %.6f\n", width, height, nprocs, slowest);

    if (do_print) {                 // collect strips on rank 0 only for printing
        unsigned char *all_right = NULL, *all_down = NULL;
        int *counts = NULL, *displs = NULL;
        if (rank == 0) {
            all_right = malloc((size_t)height * width);
            all_down = malloc((size_t)height * width);
            counts = malloc(nprocs * sizeof(int));
            displs = malloc(nprocs * sizeof(int));
            int offset = 0;
            for (int proc = 0; proc < nprocs; proc++) {   // bytes coming from each rank
                int start = proc * rows_each + (proc < leftover ? proc : leftover);
                int end = start + rows_each + (proc < leftover ? 1 : 0);
                counts[proc] = (end - start) * width;
                displs[proc] = offset;
                offset += counts[proc];
            }
        }
        MPI_Gatherv(wall_right, my_rows * width, MPI_UNSIGNED_CHAR, all_right, counts, displs, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
        MPI_Gatherv(wall_down, my_rows * width, MPI_UNSIGNED_CHAR, all_down, counts, displs, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
        if (rank == 0) {
            wall_right = all_right;
            wall_down = all_down;
            print_maze(height);
        }
    }

    MPI_Finalize();
    return 0;
}
