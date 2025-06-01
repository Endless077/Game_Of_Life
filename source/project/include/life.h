#ifndef LIFE_H
#define LIFE_H

/**
 * @file life.h
 * @brief Core API for Conway's Game of Life logic.
 *
 * This header provides functions to:
 *   - Create/destroy a random board (plain, without ghost rows).
 *   - Count alive cells in a plain board.
 *   - Advance one generation on a padded board with ghost rows (for MPI).
 */

#include <stdlib.h>

/**
 * @brief Allocate and initialize a random board (plain, size rows×cols).
 *
 * This function returns a flat array of size rows*cols. Each entry is 0 (dead)
 * or 1 (alive), chosen at random. It guarantees at least one alive cell
 * (rejection sampling if the first draw yields zero alive).
 *
 * @param rows Number of rows in the board (without ghost).
 * @param cols Number of columns in the board.
 * @param seed Random seed (passed to rand_r). If seed == 0, uses time(NULL).
 * @return Pointer to a flat array of chars of length rows*cols.
 *         Caller must free() this buffer with life_destroy().
 */
char* life_create(int rows, int cols, unsigned int seed);

/**
 * @brief Free a board previously allocated by life_create_random.
 *
 * @param board Pointer returned by life_create_random().
 */
void life_destroy(char *board);

/**
 * @brief Count alive cells in a plain board.
 *
 * @param board Pointer to a flat array of length rows*cols.
 * @param size  Total number of cells (rows*cols).
 * @return Total number of cells equal to 1.
 */
long life_count(const char *board, int size);

/**
 * @brief Compute one generation of Game of Life on a padded buffer.
 *
 * This function assumes that `current` and `next` are buffers of size
 * (rows+2) * cols. Rows 0 and rows+1 are “ghost rows” (popolate esternamente).
 * The actual data lives in rows 1..rows. For each cell (i,j) with
 * 1 <= i <= rows, 0 <= j < cols, we count its eight neighbors (handling
 * left/right columns via simple index checks, i.e., no wrap-around). The rules:
 *
 *   - If a cell is alive and has fewer than 2 alive neighbors → dies.
 *   - If a cell is alive and has 2 or 3 alive neighbors → stays alive.
 *   - If a cell is alive and has more than 3 alive neighbors → dies.
 *   - If a cell is dead and has exactly 3 alive neighbors → becomes alive.
 *
 * All updates are written to the `next` buffer, which must also be size
 * (rows+2) * cols. Ghost rows of `next` (row 0 and row rows+1) are not set
 * by this function (they will be overwritten by MPI ghost exchanges).
 *
 * @param current Pointer to current board of size (rows+2)*cols.
 * @param next    Pointer to buffer for next board, size (rows+2)*cols.
 * @param rows    Number of real rows (excludes ghost). Valid data in `current`
 *                is in rows 1..rows.  
 * @param cols    Number of columns.
 */
void life_step(const char *current, char *next, int rows, int cols);

#endif // LIFE_H
