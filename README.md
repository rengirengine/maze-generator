# Parallel maze generator

A perfect maze generator that is parallelized two ways by domain decomposition: an
OpenMP shared-memory version and an MPI distributed-memory version.

The grid is cut into horizontal strips, one per worker. Each worker carves a
perfect maze inside its strip with an iterative randomized DFS. The strips are
then joined by opening one passage between each pair of neighboring strips. With
N cells there are N-1 passages and a single connected component, so the result
is a perfect maze for any number of workers.

In the MPI version each rank stores only its own strip and the join needs no
communication: the rank above a border opens one wall at a column derived from
the shared seed. Ranks send their strips to rank 0 only when the maze is
printed. The sequential baseline is simply a run with one worker, so the same
binaries cover the whole scaling study.

## Layout

    src/        maze.c, maze.h   sequential carving + ASCII printing (no parallel code)
                maze_omp.c        OpenMP driver
                maze_mpi.c        MPI driver
    scripts/    strong.sh, weak.sh    scaling sweeps (repeat + average)
                common.sh             shared helper, sourced by the two above
                job_omp.pbs, job_mpi.pbs   PBS job scripts for the cluster
    results/    example_maze.txt, raw timing logs
    plots/      summary CSVs behind the report figures

Both drivers call `carve` from `src/maze.c`.

## Build

    make            # builds ./maze (OpenMP) and ./maze_mpi (MPI)

Compiler flags (from the Makefile): `-O3 -march=native -fopenmp -Wall` for the
OpenMP build, `-O3 -march=native -Wall` for the MPI build.

On macOS (local development only) the default `cc` is Apple Clang without
OpenMP. Build with a real OpenMP compiler and an MPI wrapper, e.g.

    make CC=clang MPICC=/opt/homebrew/opt/open-mpi/bin/mpicc

after `brew install libomp open-mpi` (or use `CC=gcc-14`).

## Run (local)

    OMP_NUM_THREADS=P ./maze WIDTH HEIGHT [SEED] [p]
    mpirun -np P ./maze_mpi WIDTH HEIGHT [SEED] [p]

Width and height are in cells. SEED makes a run reproducible. Append `p` to
print the maze as ASCII to stdout. The generation time in seconds is written to
stderr as:

    width height workers time

Examples:

    OMP_NUM_THREADS=4 ./maze 30 15 7 p
    mpirun -np 4 ./maze_mpi 4000 4000

The exact maze depends on both the seed and the worker count, since the worker
count sets the decomposition. Regenerate `results/example_maze.txt` with:

    OMP_NUM_THREADS=1 ./maze 40 20 7 p > results/example_maze.txt

## Scaling

Run the sweeps from the repository root:

    ./scripts/strong.sh 4000 4000     # fixed size (strong scaling)
    ./scripts/weak.sh   4000 4000     # grid grows with workers (weak scaling)

Each setting is run `RUNS` times and reported as the mean and sample standard
deviation. In `weak.sh` the width is
fixed and the height is rows-per-worker times the worker count.

Variables to be set from the environment:

| variable   | default     | meaning                                            |
|------------|-------------|----------------------------------------------------|
| `RUNS`     | `5`         | repetitions per setting                            |
| `WORKERS`  | `1 2 4 8`   | worker counts to sweep                             |
| `VERSIONS` | `omp mpi`   | which builds to run                                |
| `MPIFLAGS` | (empty)     | extra `mpirun` flags                               |

To scale for more than 8 workers:

    RUNS=7 WORKERS="1 2 4 8 16 32 64" ./scripts/strong.sh 8000 8000
MPI sometimes refuse to launch on a computer with fewer slots than ranks. If this happens, set
`MPIFLAGS=--oversubscribe` for **local runs only**. You should launch through the scheduler on a laptop.

## Run on the cluster (UniTN HPC)

This project can be benchmarked on the University of Trento HPC cluster. **Timing
runs must happen on compute nodes. Running jobs on the head/login nodes is not
allowed.** 

1. **Connect.** If you are not in the campus, open the university VPN. Then SSH in
   with your credentials:

        ssh username@hpc.unitn.it

2. **Copy the project up**:

        scp -r maze-generator username@hpc.unitn.it:~/

3. **Load the toolchain and find the MPI module:**

        module load gcc91      # provides gcc-9.1.0
        module avail           # note the MPI module name (an mpich/openmpi build)

   Put that MPI module name into `scripts/job_mpi.pbs` (it currently has a
   placeholder marked `VERIFY with module avail`).

4. **Check a node's core count**:

        qsub -I -q short_cpuQ -l select=1:ncpus=8:mem=1gb:walltime=00:10:00
        lscpu        # CPU model, cores/node, sockets/NUMA
        exit

   If nodes have fewer than 64 cores, lower `ncpus`/`ompthreads` and the
   `WORKERS` list in the PBS scripts to match.

5. **Submit from the repository root**:

        cd ~/maze-generator
        qsub scripts/job_omp.pbs     # OpenMP strong + weak, one node
        qsub scripts/job_mpi.pbs     # MPI strong + weak, several nodes
        qstat -u username            # watch jobs (Q queued, R running, E exiting)

6. **Collect results** from `results/maze_omp.o` / `results/maze_mpi.o`.

        scp 'username@hpc.unitn.it:~/maze-generator/results/maze_*.[oe]' ./results/

**Queues**: `debug_cpuQ` (should be less than 15 minutes, quick tests),
`short_cpuQ` (less than 6 hours), `common_cpuQ`/`long_cpuQ` for longer runs.

Nothing except the script defaults in the code limits the worker count. The OpenMP build uses `omp_get_max_threads()` (bounded
by the requested `ncpus`/`ompthreads`), so on one node you can run as many
threads as the node has cores (`select=1:ncpus=64:ompthreads=64`). The MPI build
uses the launcher's rank count; hence, it spans nodes
(`select=4:ncpus=16:mpiprocs=16` = 64 ranks. `net_type=IB` selects the
Infiniband interconnect). The provided PBS scripts sweep 1..64 workers.

by Burak Can Arikan
