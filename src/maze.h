#ifndef MAZE_H
#define MAZE_H
#include <stdint.h>

extern int width;
extern unsigned char* wall_right;   // right wall of each cell
extern unsigned char* wall_down;    // bottom wall of each cell
extern unsigned char* visited;

uint64_t next_rand(uint64_t* state);
void carve(int row_start, int row_end, uint64_t seed);
void print_maze(int height);

#endif
