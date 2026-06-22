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
                multinode.sh          same ranks on 1 vs 2 nodes (MPI only)
                common.sh             shared helper, sourced by the above
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

    RUNS=7 WORKERS="1 2 4 8 16 32 64 128" ./scripts/strong.sh 16000 16000
MPI sometimes refuses to launch on a computer with fewer slots than ranks. If this happens, set
`MPIFLAGS=--oversubscribe` for **local runs only**. Oversubscribing a laptop does not measure real
parallelism, so the 64- and 128-core numbers in the report come from the cluster, not a laptop.

## Run on the cluster (UniTN HPC)

The 64- and 128-core numbers come from the University of Trento HPC cluster
(**hpc3**: PBS, EasyBuild modules, 64-core nodes). This is where the multi-node
points actually span separate machines, so the barriers cross the network and
each node adds its own memory bandwidth — which a laptop can't show. **Timing
runs must happen on compute nodes, never on the head/login node.**

How the two jobs reach 64 and 128 cores:

| job           | cores         | layout                                               |
|---------------|---------------|------------------------------------------------------|
| `job_omp.pbs` | up to **64**  | 1 node, threads = cores (OpenMP cannot cross nodes)  |
| `job_mpi.pbs` | up to **128** | 2 nodes × 64 ranks; **64 = 1 full node, 128 = 2 nodes** |

Because nodes have 64 cores and `mpiprocs=64`, the MPI sweep is single-node up
to 64 ranks and uses **both nodes at 128 ranks**. OpenMP is shared-memory, so it
stops at one node's 64 cores; that ceiling is exactly what MPI removes by adding
a second node — the point of the multi-node run.

`job_mpi.pbs` also runs a small placement test (`scripts/multinode.sh`): the same
64 ranks on the same maze, once on **1 node** and once on **2 nodes** (32/node).
Cores are held fixed, so the difference is just the effect of using a second node
(its extra memory bandwidth versus the cross-node barrier cost) — something
OpenMP can't do at all.

1. **Connect.** If you are not on campus, open the university VPN. Then SSH in
   with your credentials:

        ssh username@hpc.unitn.it       # lands on hpc3-login0

2. **Copy the project up**:

        scp -r maze-generator username@hpc.unitn.it:~/

3. **Toolchain.** hpc3 uses EasyBuild modules; the scripts already load them:
   `foss/2023a` (GCC 12.3.0 + OpenMPI 4.1.5, gives `gcc` and `mpicc`/`mpirun`)
   for the MPI job and `GCC/12.3.0` for the OpenMP job. Confirm they exist with

        module -t avail 2>&1 | grep -iE '^(GCC/|OpenMPI/|foss/)'

   and edit the `module load` line in the PBS scripts if your versions differ.

4. **Node core count** (already assumed to be 64). Verify with:

        pbsnodes -a | grep -m1 resources_available.ncpus
        # or interactively:
        qsub -I -q shortCPUQ -l select=1:ncpus=8:mem=1gb:walltime=00:10:00
        lscpu ; exit

   If a node has fewer than 64 cores, lower `ncpus`/`ompthreads` and the OpenMP
   `WORKERS` list to match, and adjust `mpiprocs`/`select` in `job_mpi.pbs` so the
   run still totals 128 ranks across the nodes. (The header of `job_mpi.pbs` also
   notes an 8-node × 16-rank layout, which gives more cross-node points but is
   harder to schedule on a busy cluster.)

5. **Submit from the repository root**:

        cd ~/maze-generator
        qsub scripts/job_omp.pbs     # OpenMP strong + weak, one node, 1..64
        qsub scripts/job_mpi.pbs     # MPI strong + weak, two nodes, 1..128
        qstat -u username            # watch jobs (Q queued, R running, E exiting)

   The MPI job requests two nodes and may queue for a while; the OpenMP job
   (one node) usually starts sooner.

6. **Collect results** from `results/maze_omp.o` / `results/maze_mpi.o`:

        scp 'username@hpc.unitn.it:~/maze-generator/results/maze_*.[oe]' ./results/

   `maze_mpi.o` starts with a node-placement banner (proving 64 ran on one node
   and 128 spanned two), then the `# strong scaling …` and `# weak scaling …`
   tables (`worker  mean  std  n`, seconds), and finally the
   `# multi-node placement …` table comparing `64@1n`, `64@2n`, `128@2n`.

**Queues** (PBS, from `qstat -Q`): `shortCPUQ` (used here), `commonCPUQ` and
`longCPUQ` for longer jobs.

Nothing except the script defaults limits the worker count. The OpenMP build
uses `omp_get_max_threads()` (bounded by the requested `ncpus`/`ompthreads`), so
on one node it runs as many threads as the node has cores
(`select=1:ncpus=64:ompthreads=64`). The MPI build uses the launcher's rank
count and therefore spans nodes (`select=2:ncpus=64:mpiprocs=64` = 128 ranks,
`place=scatter` puts one chunk per node). The provided PBS scripts sweep 1..64
(OpenMP) and 1..128 (MPI).

by Burak Can Arikan
