//   ____    ____  _______  _____                    
//  |_   \  /   _||_   __ \|_   _|                   
//    |   \/   |    | |__) | | |   _   __     .---.  
//    | |\  /| |    |  ___/  | |  [ \ [  ]   / /'`\] 
//   _| |_\/_| |_  _| |_    _| |_  > '  <  _ | \__.  
//  |_____||_____||_____|  |_____|[__]`\_](_)'.___.' 
//                                                   

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpix.h"

/* ********************************************************************************************* */

void mpi_exchange_ghosts(char *buf,
                         int local_rows,
                         int cols,
                         MPI_Comm comm) {
    
    // Init current rank 
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    // Get previus and next process rank
    int rank_prev = (rank - 1 + size) % size;
    int rank_next = (rank + 1) % size;

    MPI_Status status;

    // Send first real row to rank_prev, receive bottom ghost from rank_next
    MPI_Sendrecv(
        buf + cols,                       // send buffer: first real row
        cols,                             // send count
        MPI_CHAR,                         // send datatype
        rank_prev,                        // dest rank
        0,                                // send tag
        buf + (local_rows + 1) * cols,    // recv buffer: bottom ghost
        cols,                             // recv count
        MPI_CHAR,                         // recv datatype
        rank_next,                        // source rank
        0,                                // recv tag
        comm,
        &status
    );

    // Send last real row to rank_next, receive top ghost from rank_prev
    MPI_Sendrecv(
        buf + local_rows * cols,          /* send buffer: last real row */
        cols,                             /* send count */
        MPI_CHAR,                         /* send datatype */
        rank_next,                        /* dest rank */
        1,                                /* send tag */
        buf,                              /* recv buffer: top ghost */
        cols,                             /* recv count */
        MPI_CHAR,                         /* recv datatype */
        rank_prev,                        /* source rank */
        1,                                /* recv tag */
        comm,
        &status
    );
}

void mpi_scatter_board(char *full_board,
                       int rows,
                       int cols,
                       char **local,
                       int *local_rows,
                       MPI_Comm comm) {

    // Init current rank 
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    // Calculate how many rows per rank
    int base  = rows / size;
    int extra = rows % size;

    int *sendcounts = NULL;
    int *displs     = NULL;

    // Master initialization
    if (rank == 0) {
        sendcounts = malloc(size * sizeof(int));
        displs     = malloc(size * sizeof(int));
        
        int offset = 0, total = 0;
        for (int r = 0; r < size; r++) {
            int r_rows      = base + (r < extra ? 1 : 0);
            sendcounts[r]   = r_rows * cols;    // number of chars for rank r
            displs[r]       = offset;           // starting index in full_board
            offset         += sendcounts[r];
            total          += sendcounts[r];
        }

        if (total != rows * cols) {
            fprintf(stderr,
                    "Error: sum(sendcounts) = %d but expected %d\n",
                    total, rows * cols);
            MPI_Abort(comm, EXIT_FAILURE);
        }
    }

    // Determine how many real rows this rank gets
    *local_rows = base + (rank < extra ? 1 : 0);
    int padded_elems = (*local_rows + 2) * cols;

    // Allocate padded buffer: two ghost rows + local_rows real rows
    *local = malloc(padded_elems * sizeof(char));
    if (!*local) {
        fprintf(stderr, "Error: malloc failed in mpi_scatter_board on rank %d\n", rank);
        MPI_Abort(comm, EXIT_FAILURE);
    }

    // Scatter real rows into the middle of *local (skip top ghost row)
    char *recv_ptr = (*local) + cols;
    MPI_Scatterv(
        full_board,             // send buffer (only valid on MASTER)
        sendcounts,             // array of sendcounts[r] = (r_rows * cols)
        displs,                 // array of displacements in full_board
        MPI_CHAR,               // send datatype
        recv_ptr,               // recv buffer: &((*local)[cols])
        (*local_rows) * cols,   // recv count: local_rows * cols
        MPI_CHAR,               // recv datatype */
        0,                      // root rank = MASTER */
        comm
    );

    if (rank == 0) {
        free(sendcounts);
        free(displs);
    }
}

long mpi_reduce_count(long local_count, MPI_Comm comm) {

    // Init current rank 
    int rank;
    MPI_Comm_rank(comm, &rank);

    // Initialize variable to hold the global sum
    // (meaningful only on rank 0)
    long global_count = 0;
    MPI_Reduce(
        &local_count,    // send buffer: each rank’s local alive-cell count
        &global_count,   // recv buffer: will hold the total sum on rank 0
        1,               // number of elements to reduce
        MPI_LONG,        // datatype of the elements being reduced
        MPI_SUM,         // reduction operation: sum all local_count values
        0,               // root rank: only rank 0 receives the result
        comm             // communicator over which to perform the reduction
    );

    // Return the count, only rank 0 returns the
    // summed global count, others return 0
    return (rank == 0) ? global_count : 0;
}

int mpi_check_steady_state(const char *current,
                           const char *next,
                           int local_rows,
                           int cols,
                           MPI_Comm comm) {
    int local_changed = 0;

    // Loop over each real row, break early if a change is found
    for (int i = 1; i <= local_rows && !local_changed; i++) {
        for (int j = 0; j < cols; j++) {
            // Compare cell at (i,j) in current and next buffers
            if (next[i * cols + j] != current[i * cols + j]) {
                local_changed = 1;  // Mark that this rank has at least one changed cell
                break;
            }
        }
    }

    int global_changed = 0;
    // Perform a logical OR reduction across all ranks:
    // if any rank's local_changed == 1, then global_changed becomes 1
    MPI_Allreduce(&local_changed,    // send buffer (int: 0 or 1)
                  &global_changed,   // receive buffer (int: result of MPI_LOR)
                  1,                 // number of elements
                  MPI_INT,           // datatype of each element
                  MPI_LOR,           // logical OR operation
                  comm);             // communicator

    // If no rank detected a change (global_changed == 0), the board is stable
    // Return 1 for stable, 0 for changed
    return (global_changed == 0) ? 1 : 0; // 1 = stable, 0 = changed
}

int mpi_check_zero_population(const char *current,
                              int local_rows,
                              int cols,
                              MPI_Comm comm) {
    long local_alive = 0;
    
    // Count alive cells in real rows only (rows 1..local_rows)
    for (int i = 1; i <= local_rows; i++) {
        for (int j = 0; j < cols; j++) {
            // Increment if this cell is alive (value == 1)
            local_alive += (current[i * cols + j] == 1);
        }
    }

    // If no alive cells found locally, set local_zero = 1; else 0
    int local_zero = (local_alive == 0) ? 1 : 0;
    int global_zero = 0;

    // Perform a logical AND reduction across all ranks:
    // if every rank has local_zero == 1, global_zero becomes 1
    MPI_Allreduce(&local_zero,    // send buffer (int: 0 or 1)
                  &global_zero,   // receive buffer (int: result of MPI_LAND)
                  1,              // number of elements to reduce
                  MPI_INT,        // datatype of each element
                  MPI_LAND,       // logical AND operation
                  comm);          // communicator

    // Return 1 if the entire board has zero population (global_zero == 1), else 0
    return (global_zero == 1) ? 1 : 0; // 1 = zero population, 0 = still alive
}

/* ********************************************************************************************* */
