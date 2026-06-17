#!/bin/sh
# Weak scaling: work per worker held constant (height grows with workers).
# Each setting is timed $RUNS times and reported as mean and sample std.
#   ./scripts/weak.sh [WIDTH] [ROWS_PER_WORKER]
#   RUNS=7 WORKERS="1 2 4 8 16 32 64" ./scripts/weak.sh 8000 1000
# On a laptop with fewer slots than ranks, add MPIFLAGS=--oversubscribe.
. "$(dirname "$0")/common.sh"
W="${1:-4000}"; R="${2:-4000}"

echo "# weak scaling  ${W} wide, ${R} rows/worker  runs=${RUNS}"
printf '%-8s %9s %9s %3s\n' worker mean std n
if want omp; then
    for p in $WORKERS; do
        measure "omp-$p" -- env OMP_NUM_THREADS="$p" "$MAZE" "$W" "$((R*p))" "$SEED"
    done
fi
if want mpi; then
    for p in $WORKERS; do
        measure "mpi-$p" -- $MPIRUN $MPIFLAGS -np "$p" "$MAZE_MPI" "$W" "$((R*p))" "$SEED"
    done
fi
