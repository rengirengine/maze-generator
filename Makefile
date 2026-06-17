CC       ?= cc
MPICC    ?= mpicc
CFLAGS   ?= -O3 -march=native -fopenmp -Wall
MPICFLAGS?= -O3 -march=native -Wall
SRC      := src

all: maze maze_mpi

maze: $(SRC)/maze.c $(SRC)/maze_omp.c $(SRC)/maze.h
	$(CC) $(CFLAGS) -I$(SRC) -o maze $(SRC)/maze.c $(SRC)/maze_omp.c

maze_mpi: $(SRC)/maze.c $(SRC)/maze_mpi.c $(SRC)/maze.h
	$(MPICC) $(MPICFLAGS) -I$(SRC) -o maze_mpi $(SRC)/maze.c $(SRC)/maze_mpi.c

clean:
	rm -f maze maze_mpi
