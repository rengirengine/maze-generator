#!/bin/sh
# Strong scaling: fixed problem size, increasing worker count.
# Each setting is timed $RUNS times and reported as mean and sample std.
#   ./scripts/strong.sh [WIDTH] [HEIGHT]
#   RUNS=7 WORKERS="1 2 4 8 16 32 64" ./scripts/strong.sh 8000 8000
# On a laptop with fewer slots than ranks, add MPIFLAGS=--oversubscribe.
. "$(dirname "$0")/common.sh"
W="${1:-4000}"; H="${2:-4000}"

echo "# strong scaling  ${W}x${H}  runs=${RUNS}"
printf '%-8s %9s %9s %3s\n' worker mean std n
if want omp; then
    for p in $WORKERS; do
        measure "omp-$p" -- env OMP_NUM_THREADS="$p" "$MAZE" "$W" "$H" "$SEED"
    done
fi
if want mpi; then
    for p in $WORKERS; do
        measure "mpi-$p" -- $MPIRUN $MPIFLAGS -np "$p" "$MAZE_MPI" "$W" "$H" "$SEED"
    done
fi
