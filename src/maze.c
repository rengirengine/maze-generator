#include <stdio.h>
#include <stdlib.h>
#include "maze.h"

int width;
unsigned char* wall_right;   // right wall of each cell
unsigned char* wall_down;    // bottom wall of each cell
unsigned char* visited;

uint64_t next_rand(uint64_t* state) {   // xorshift64
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

static void open_wall(int cell, int neighbour) {   // remove the wall between them
    if (cell / width == neighbour / width) {       // same row -> a right wall
        if (neighbour > cell) wall_right[cell] = 0;
        else                  wall_right[neighbour] = 0;
    }
    else {                                         // stacked rows -> a bottom wall
        if (neighbour > cell) wall_down[cell] = 0;
        else                  wall_down[neighbour] = 0;
    }
}

void carve(int row_start, int row_end, uint64_t seed) {   // randomized dfs over one strip
    int strip_cells = (row_end - row_start) * width;
    int* stack = malloc(strip_cells * sizeof(int));
    int stack_top = 0;

    int start_cell = row_start * width;
    visited[start_cell] = 1;
    stack[stack_top++] = start_cell;

    while (stack_top > 0) {
        int current = stack[stack_top - 1];
        int row = current / width, col = current % width;

        int neighbours[4], count = 0;   // unvisited cells next to current, inside the strip
        if (row > row_start && !visited[current - width]) {
            neighbours[count++] = current - width;
        }
        if (row < row_end - 1 && !visited[current + width]) {
            neighbours[count++] = current + width;
        }
        if (col > 0 && !visited[current - 1]) {
            neighbours[count++] = current - 1;
        }
        if (col < width - 1 && !visited[current + 1]) {
            neighbours[count++] = current + 1;
        }

        if (count == 0) {
            stack_top--;                // dead end -> step back
            continue;
        }

        int chosen = neighbours[next_rand(&seed) % count];   // pick a random direction
        open_wall(current, chosen);
        visited[chosen] = 1;
        stack[stack_top++] = chosen;
    }
    free(stack);
}

void print_maze(int height) {   // ascii render to stdout
    for (int col = 0; col < width; col++) fputs("+--", stdout);
    fputs("+\n", stdout);
    for (int row = 0; row < height; row++) {
        putchar('|');
        for (int col = 0; col < width; col++) {
            fputs("  ", stdout);
            putchar(wall_right[row * width + col] ? '|' : ' ');
        }
        putchar('\n');
        for (int col = 0; col < width; col++) {
            putchar('+');
            fputs(wall_down[row * width + col] ? "--" : "  ", stdout);
        }
        fputs("+\n", stdout);
    }
}
