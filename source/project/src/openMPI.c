#include "openMPI.h"
#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>

/**
 * @brief Distribute rows of the board from MASTER to all ranks (row-based).
 *
 * On MASTER, full_board points to the plain board of size rows*cols. On other ranks, full_board is NULL.
 * This function calculates sendcounts and displacements so that rows are divided as evenly as possible.
 * It allocates a padded buffer of (local_rows+2)*cols for each rank, performs MPI_Scatterv to fill
 * the real rows into local[1..local_rows], and leaves the ghost rows uninitialized for later exchange.
 */
void mpi_scatter_board(char *full_board,
                       int rows,
                       int cols,
                       char **local,
                       int *local_rows,
                       MPI_Comm comm) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    // Compute number of real rows per rank
    int base = rows / size;
    int extra = rows % size;

    int *sendcounts = NULL;
    int *displs     = NULL;

    if (rank == 0) {
        // Allocate arrays on MASTER
        sendcounts = (int *)malloc(size * sizeof(int));
        displs     = (int *)malloc(size * sizeof(int));

        int offset = 0;
        int total   = 0;
        for (int r = 0; r < size; r++) {
            int rows_r       = base + (r < extra ? 1 : 0);
            sendcounts[r] = rows_r * cols;  // number of chars to send
            displs[r]     = offset;         // displacement in chars
            offset          += sendcounts[r];
            total           += sendcounts[r];
        }

        // Verify sum of sendcounts matches total cells
        if (total != rows * cols) {
            fprintf(stderr,
                    "Error: sum(sendcounts) = %d but expected %d\n",
                    total, rows * cols);
            MPI_Abort(comm, EXIT_FAILURE);
        }
    }

    // Determine local_rows for this rank
    *local_rows = base + (rank < extra ? 1 : 0);

    // Allocate padded buffer: (local_rows + 2) * cols
    int padded_elems = (*local_rows + 2) * cols;
    *local = (char *)malloc(padded_elems * sizeof(char));
    if (!*local) {
        fprintf(stderr, "Error: malloc failed in mpi_scatter_board on rank %d\n", rank);
        MPI_Abort(comm, EXIT_FAILURE);
    }

    // Pointer to where the real data should go (skip top ghost row)
    char *recv_ptr = (*local) + cols;

    // Scatter the board: MASTER distributes, others receive
    MPI_Scatterv(full_board,
                 sendcounts,
                 displs,
                 MPI_CHAR,
                 recv_ptr,
                 (*local_rows) * cols,
                 MPI_CHAR,
                 0,
                 comm);

    // Free sendcounts/displs on MASTER
    if (rank == 0) {
        free(sendcounts);
        free(displs);
    }
}

/**
 * @brief Exchange ghost rows with neighbor ranks (row-based, cyclic).
 *
 * This function updates the top and bottom ghost rows of the padded buffer buf
 * by sending and receiving the adjacent real rows from neighboring ranks.
 */
// src/openMPI.c

#include "openMPI.h"
#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>

/**
 * @brief Exchange ghost rows with neighbor ranks (row-based, cyclic).
 *
 * Now ogni Sendrecv corrisponde esattamente a un Sendrecv opposto:
 *  - Il primo Sendrecv manda la prima riga reale a rank_prev e riceve
 *    la riga inferiore da rank_next (tag 0).
 *  - Il secondo Sendrecv manda l'ultima riga reale a rank_next e riceve
 *    la riga superiore da rank_prev (tag 1).
 */
void mpi_exchange_ghosts(char *buf,
                         int local_rows,
                         int cols,
                         MPI_Comm comm) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    int rank_prev = (rank - 1 + size) % size;
    int rank_next = (rank + 1) % size;

    MPI_Status status;

    // 1) Send first real row (buf + cols) a rank_prev, receive riga inferiore da rank_next
    printf("DEBUG (mpi_exchange): [Rank %d] entering Sendrecv TOP→PREV (recv from NEXT=%d)\n",
           rank, rank_next);
    fflush(stdout);
    MPI_Sendrecv(
        buf + cols,                        // send: first real row
        cols, MPI_CHAR,
        rank_prev, 0,                      // dest=rank_prev, tag=0
        buf + (local_rows + 1) * cols,     // recv: bottom ghost (dalla riga inferiore di rank_next)
        cols, MPI_CHAR,
        rank_next, 0,                      // source=rank_next, tag=0
        comm,
        &status
    );
    printf("DEBUG (mpi_exchange): [Rank %d] completed Sendrecv TOP↔BOTTOM (prev=%d, next=%d)\n",
           rank, rank_prev, rank_next);
    fflush(stdout);

    // 2) Send last real row (buf + local_rows*cols) a rank_next, receive riga superiore da rank_prev
    printf("DEBUG (mpi_exchange): [Rank %d] entering Sendrecv BOTTOM→NEXT (recv from PREV=%d)\n",
           rank, rank_prev);
    fflush(stdout);
    MPI_Sendrecv(
        buf + (local_rows) * cols,         // send: last real row
        cols, MPI_CHAR,
        rank_next, 1,                      // dest=rank_next, tag=1
        buf,                               // recv: top ghost (dalla riga superiore di rank_prev)
        cols, MPI_CHAR,
        rank_prev, 1,                      // source=rank_prev, tag=1
        comm,
        &status
    );
    printf("DEBUG (mpi_exchange): [Rank %d] completed Sendrecv BOTTOM↔TOP (prev=%d, next=%d)\n",
           rank, rank_prev, rank_next);
    fflush(stdout);
}

/**
 * @brief Gather global alive-cell count via MPI_Reduce.
 *
 * Each rank passes its local_count (number of alive cells in its real rows).
 * This function performs an MPI_Reduce with MPI_SUM, root at rank 0.
 * On MASTER (rank 0) returns the total alive-cell count; on other ranks returns 0.
 */
long mpi_reduce_count(long local_count,
                      MPI_Comm comm) {
    int rank;
    MPI_Comm_rank(comm, &rank);

    long global_count = 0;
    MPI_Reduce(&local_count,
               &global_count,
               1,           // count
               MPI_LONG,    // data type
               MPI_SUM,     // operation
               0,           // root rank
               comm);

    return (rank == 0) ? global_count : 0;
}
