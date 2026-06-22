#!/bin/sh
# Same maze and same total ranks, different number of nodes (1 vs 2 here): shows
# the cost/benefit of spreading ranks across nodes on its own, with cores held
# fixed. Ranks-per-node come from a per-config hostfile with the default mapping.
# Not --bind-to/--map-by ppr: this OpenMPI has no PBS integration and mis-binds
# remote ranks, which wrecks the timing. Needs $PBS_NODEFILE with >=2 nodes.
#   RUNS=7 ./scripts/multinode.sh 16000 16000
. "$(dirname "$0")/common.sh"
W="${1:-16000}"; H="${2:-16000}"

[ -n "$PBS_NODEFILE" ] || { echo "multinode.sh: no \$PBS_NODEFILE (cluster only)" >&2; exit 1; }
N1=$(sort -u "$PBS_NODEFILE" | sed -n 1p)
N2=$(sort -u "$PBS_NODEFILE" | sed -n 2p)
[ -n "$N2" ] || { echo "multinode.sh: need >=2 distinct nodes" >&2; exit 1; }
TMP="${TMPDIR:-/tmp}/mn.$$"

cfg() { printf '%s\n' "$@" > "$TMP"; echo "$TMP"; }   # write hostfile, echo its path
run() { measure "$1" -- $MPIRUN --hostfile "$3" -np "$2" "$MAZE_MPI" "$W" "$H" "$SEED"; }

echo "# multi-node placement  ${W}x${H}  runs=${RUNS}  (same ranks, varying #nodes)"
printf '%-8s %9s %9s %3s\n' config mean std n
run "64@1n"  64  "$(cfg "$N1 slots=64")"                      # 64 ranks, one node
run "64@2n"  64  "$(cfg "$N1 slots=32" "$N2 slots=32")"       # 64 ranks, two nodes (32/node)
run "128@2n" 128 "$(cfg "$N1 slots=64" "$N2 slots=64")"       # 128 ranks, two nodes
rm -f "$TMP"
