# Shared setup for the scaling scripts. This file is sourced, not run.
#
# Tunables (all overridable from the environment):
#   RUNS      repetitions per setting          (default 5)
#   WORKERS   worker counts to sweep           (default "1 2 4 8")
#   VERSIONS  which builds to run: omp, mpi     (default "omp mpi")
#   MPIFLAGS  extra mpirun flags, e.g. --oversubscribe on a laptop (default none)
#   MPIRUN    mpirun launcher                  (default "mpirun")
#   MAZE / MAZE_MPI   binary paths             (default repo-root ./maze[_mpi])

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MAZE="${MAZE:-$ROOT/maze}"
MAZE_MPI="${MAZE_MPI:-$ROOT/maze_mpi}"
MPIRUN="${MPIRUN:-mpirun}"
RUNS="${RUNS:-5}"
WORKERS="${WORKERS:-1 2 4 8}"
VERSIONS="${VERSIONS:-omp mpi}"
MPIFLAGS="${MPIFLAGS:-}"
SEED=1234

want() { case " $VERSIONS " in *" $1 "*) return 0 ;; *) return 1 ;; esac; }

want omp && [ ! -x "$MAZE" ]     && { echo "missing $MAZE (run: make)" >&2; exit 1; }
want mpi && [ ! -x "$MAZE_MPI" ] && { echo "missing $MAZE_MPI (run: make)" >&2; exit 1; }

# measure LABEL -- COMMAND...
# Runs COMMAND $RUNS times. The binaries print "W H P seconds" to stderr; we
# read the seconds field and print "LABEL mean std n" (times in seconds).
measure() {
    label="$1"; shift 2                 # drop LABEL and the "--"
    times=""
    for _ in $(seq 1 "$RUNS"); do
        t=$("$@" 2>&1 | awk '$1 ~ /^[0-9]+$/ && NF==4 {print $4}')
        times="$times $t"
    done
    printf '%s' "$times" | awk -v l="$label" '{
        n=NF; s=0; for (i=1;i<=n;i++) s+=$i; m=s/n;
        v=0; for (i=1;i<=n;i++) v+=($i-m)*($i-m);
        sd=(n>1)?sqrt(v/(n-1)):0;
        printf "%-8s %9.4f %9.4f %3d\n", l, m, sd, n;
    }'
}
