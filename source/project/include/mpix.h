//   ____    ____  _______  _____             __       
//  |_   \  /   _||_   __ \|_   _|           [  |      
//    |   \/   |    | |__) | | |   _   __     | |--.   
//    | |\  /| |    |  ___/  | |  [ \ [  ]    | .-. |  
//   _| |_\/_| |_  _| |_    _| |_  > '  <  _  | | | |  
//  |_____||_____||_____|  |_____|[__]`\_](_)[___]|__] 
//                                                     

#ifndef MPIX_H
#define MPIX_H

#include <mpi.h>

/**
 * @brief Exchange ghost rows with neighbor ranks (row-based, cyclic).
 *
 * Given a padded buffer buf of size (local_rows+2)*cols:
 *   - buf[0..cols-1] is the top ghost row.
 *   - buf[cols..local_rows*cols+cols-1] are the real rows.
 *   - buf[(local_rows+1)*cols..end] is the bottom ghost row.
 *
 * This function:
 *   1. Sends first real row to rank_prev, receives bottom ghost from rank_next.
 *   2. Sends last real row to rank_next, receives top ghost from rank_prev.
 *
 * @param buf         Padded buffer ((local_rows+2)*cols).
 * @param local_rows  Number of real rows (excluding ghosts).
 * @param cols        Number of columns.
 * @param comm        MPI communicator.
 */
void mpi_exchange_ghosts(char *buf,
                         int local_rows,
                         int cols,
                         MPI_Comm comm);


/**
 * @brief Distribute rows of the board from MASTER to all ranks (row-based).
 *
 * On MASTER (rank 0), full_board points to a plain array of size rows*cols.
 * Other ranks pass full_board = NULL. Each rank receives local_rows rows into
 * a padded buffer of size (local_rows+2)*cols, with two ghost rows to be filled
 * by mpi_exchange_ghosts().
 *
 * @param full_board   On MASTER: pointer to plain board (rows*cols). Others: NULL.
 * @param rows         Total number of rows in full_board.
 * @param cols         Total number of columns.
 * @param local        OUT: pointer to newly allocated padded buffer.
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
 * @brief Gather global alive-cell count via MPI_Reduce.
 *
 * Each rank calls this with its local count; MASTER (rank 0) receives
 * the sum of all local counts, others return 0.
 *
 * @param local_count Number of alive cells in this rank’s real rows.
 * @param comm        MPI communicator.
 * @return Total alive cells on MASTER; 0 on other ranks.
 */
long mpi_reduce_count(long local_count, MPI_Comm comm);


/**
 * @brief Check if the board has reached a steady state (no cell changed).
 *
 * Each rank compares its “real” rows in current[] vs next[]. If any difference
 * is found, local_changed=1; then an MPI_Allreduce(MPI_LOR) across all ranks
 * yields global_changed. If global_changed==0, the board is stable.
 *
 * @param current     Padded buffer holding current generation ((local_rows+2)*cols).
 * @param next        Padded buffer holding next generation ((local_rows+2)*cols).
 * @param local_rows  Number of real rows per rank.
 * @param cols        Number of columns.
 * @param comm        MPI communicator.
 * @return 1 if stable (no changes), 0 otherwise.
 */
int mpi_check_steady_state(const char *current,
                           const char *next,
                           int local_rows,
                           int cols,
                           MPI_Comm comm);

/**
 * @brief Check if the global population is zero.
 *
 * Each rank counts its own alive cells (in current[1..local_rows] rows),
 * sets local_zero=1 if count==0, then MPI_Allreduce(MPI_LAND) yields
 * global_zero == 1 iff every rank has zero alive cells.
 *
 * @param current     Padded buffer holding current generation ((local_rows+2)*cols).
 * @param local_rows  Number of real rows per rank.
 * @param cols        Number of columns.
 * @param comm        MPI communicator.
 * @return 1 if global population is zero, 0 otherwise.
 */
int mpi_check_zero_population(const char *current,
                              int local_rows,
                              int cols,
                              MPI_Comm comm);

#endif // MPIX_H
