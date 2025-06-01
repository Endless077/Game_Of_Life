#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include "life.h"
#include "openMPI.h"
#include "utils.h"

/**
 * @file main.c
 * @brief Entry point for the MPI Game of Life simulation.
 */

static void print_usage(const char *prog_name) {
    fprintf(stderr,
        "Usage: %s -n <rows> -m <cols> -i <iterations> [-s <seed>]\n"
        "  -n <rows>        Number of rows in the board\n"
        "  -m <cols>        Number of columns in the board\n"
        "  -i <iterations>  Number of simulation iterations\n"
        "  -s <seed>        Optional random seed\n",
        prog_name);
}

int main(int argc, char *argv[]) {
    int rows = 0, cols = 0, iterations = 0;
    int user_seed = 0;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i+1 < argc) {
            rows = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-m") == 0 && i+1 < argc) {
            cols = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-i") == 0 && i+1 < argc) {
            iterations = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-s") == 0 && i+1 < argc) {
            user_seed = atoi(argv[++i]);
        } else {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }
    if (rows <= 0 || cols <= 0 || iterations <= 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    // Initialize MPI
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Debug: rank started
    printf("DEBUG: [Rank %d/%d] started, rows=%d cols=%d iterations=%d seed=%d\n",
           rank, size, rows, cols, iterations, user_seed);
    fflush(stdout);

    // Initialize seed
    unsigned int seed = init_seed(user_seed + rank);
    if (rank == 0) {
        printf("Using base seed: %d (rank0 uses %u)\n", user_seed, seed);
        fflush(stdout);
    }

    // Master creates full board
    char *full_board = NULL;
    if (rank == 0) {
        full_board = life_create(rows, cols, seed);
        if (!full_board) {
            fprintf(stderr, "Error: allocation failed on MASTER\n");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
    }

    // Scatter board
    char *local_buf = NULL;
    int local_rows = 0;
    mpi_scatter_board(full_board, rows, cols, &local_buf, &local_rows, MPI_COMM_WORLD);

    // Debug: print local_rows
    printf("DEBUG: [Rank %d] local_rows = %d\n", rank, local_rows);
    fflush(stdout);

    if (rank == 0) {
        free(full_board);
    }

    // Allocate buffers
    int padded_rows = local_rows + 2;
    size_t buf_size = (size_t)padded_rows * cols * sizeof(char);
    char *current = malloc(buf_size);
    char *next    = malloc(buf_size);
    if (!current || !next) {
        fprintf(stderr, "Error: allocation failed on rank %d\n", rank);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    // Copy initial data and zero ghosts
    memcpy(current + cols, local_buf + cols, (size_t)local_rows * cols * sizeof(char));
    free(local_buf);
    memset(current, 0, cols);
    memset(current + (local_rows + 1)*cols, 0, cols);

    double start_time = get_time();
    for (int gen = 1; gen <= iterations; gen++) {
        // Init barrier to synchronize before exchange
        MPI_Barrier(MPI_COMM_WORLD);

        if (rank == 0) {
            printf("DEBUG: [Rank 0] barrier passed, gen=%d\n", gen);
            fflush(stdout);
        }

        // Exchange ghosts
        mpi_exchange_ghosts(current, local_rows, cols, MPI_COMM_WORLD);

        if (rank == 0) {
            printf("DEBUG: [Rank 0] after mpi_exchange_ghosts(), gen=%d\n", gen);
            fflush(stdout);
        }

        // Compute next generation
        life_step(current, next, local_rows, cols);

        if (rank == 0) {
            printf("DEBUG: [Rank 0] after life_step(), gen=%d\n", gen);
            fflush(stdout);
        }

        // Swap
        char *tmp = current;
        current = next;
        next = tmp;

        // Gather stats
        long local_alive  = life_count(current + cols, local_rows * cols);
        long global_alive = mpi_reduce_count(local_alive, MPI_COMM_WORLD);
        if (rank == 0) {
            double t_now = get_time();
            printf("[Gen %4d] Alive cells = %ld  Elapsed=%.4f s\n",
                   gen, global_alive, t_now - start_time);
            fflush(stdout);
        }
    }

    if (rank == 0) {
        double total_time = get_time() - start_time;
        printf("Simulation complete: %d generations, %dx%d board, %d ranks.\n",
               iterations, rows, cols, size);
        printf("Total time: %.4f s  Avg per gen: %.6f s\n",
               total_time, total_time / iterations);
        fflush(stdout);
    }

    free(current);
    free(next);
    MPI_Finalize();
    return 0;
}