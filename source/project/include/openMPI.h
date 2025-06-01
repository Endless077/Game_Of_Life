// include/mpi.h

#ifndef OPEN_MPI_H
#define OPEN_MPI_H

/**
 * @file mpi.h
 * @brief Helpers for splitting the board across MPI ranks, exchanging ghost rows, and gathering stats.
 *
 * Provides functions to:
 *   - Distribute (scatter) the full plain board (rows×cols) row-by-row among P ranks,
 *     each rank receives its own real rows into a padded buffer with ghost rows.
 *   - Exchange the ghost rows with adjacent ranks in a cyclic fashion.
 *   - Reduce (MPI_Reduce) the local alive-cell count to obtain the global total on the MASTER.
 */

#include <mpi.h>

/**
 * @brief Distribute rows of the board from MASTER to all ranks (row-based).
 *
 * On MASTER (rank 0), full_board points to a plain board array of size rows*cols.
 * Other ranks pass full_board = NULL. This function:
 *   1. Computes sendcounts[] and displs[] so that each rank gets a roughly equal number of rows.
 *   2. Allocates *local as a padded buffer of size (local_rows + 2) * cols:
 *        *local = malloc((local_rows + 2) * cols * sizeof(char));
 *   3. Calls MPI_Scatterv to fill local[1..local_rows] with real data.
 *   4. Leaves local[0] (top ghost row) and local[local_rows+1] (bottom ghost row) uninitialized;
 *      they will be filled by mpi_exchange_ghosts() before each step.
 *
 * @param full_board   On MASTER: pointer to plain board (rows*cols). On other ranks: NULL.
 * @param rows         Total number of rows in full_board.
 * @param cols         Total number of columns.
 * @param local        OUT: pointer to the newly allocated padded buffer.
 * @param local_rows   OUT: number of real rows assigned to this rank.
 * @param comm         MPI communicator (e.g., MPI_COMM_WORLD).
 */
void mpi_scatter_board(char *full_board,
                       int rows,
                       int cols,
                       char **local,
                       int *local_rows,
                       MPI_Comm comm);

/**
 * @brief Exchange ghost rows with neighbor ranks (row-based, cyclic).
 *
 * Given a padded buffer buf of size (local_rows + 2)*cols:
 *   - buf[0..cols-1] is the top ghost row (to be received).
 *   - buf[cols..local_rows*cols+cols-1] are the real rows.
 *   - buf[(local_rows+1)*cols..end] is the bottom ghost row (to be received).
 *
 * This function:
 *   1. Computes rank_prev = (rank - 1 + size) % size and rank_next = (rank + 1) % size.
 *   2. Sends the first real row (buf + cols) to rank_prev and receives rank_prev’s last real row
 *      into buf + (local_rows+1)*cols (bottom ghost).
 *   3. Sends the last real row (buf + local_rows*cols) to rank_next and receives rank_next’s first real row
 *      into buf + 0 (top ghost).
 *
 * Implemented with two MPI_Sendrecv calls.
 *
 * @param buf         Pointer to the padded buffer ((local_rows+2)*cols).
 * @param local_rows  Number of real rows (excluding ghost).
 * @param cols        Number of columns.
 * @param comm        MPI communicator (e.g., MPI_COMM_WORLD).
 */
void mpi_exchange_ghosts(char *buf,
                         int local_rows,
                         int cols,
                         MPI_Comm comm);

/**
 * @brief Gather global alive-cell count via MPI_Reduce.
 *
 * Each rank provides local_count = number of alive cells in its real rows.
 * This function calls MPI_Reduce(..., MPI_SUM, 0) and returns global_count on MASTER;
 * other ranks return 0.
 *
 * @param local_count Number of alive cells counted locally (this rank).
 * @param comm        MPI communicator (e.g., MPI_COMM_WORLD).
 * @return            Total number of alive cells on MASTER; 0 on other ranks.
 */
long mpi_reduce_count(long local_count,
                      MPI_Comm comm);

#endif // OPEN_MPI_H
