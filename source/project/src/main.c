// src/main.c

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
 *
 * Parses command-line arguments manually, initializes MPI, distributes the board,
 * runs the simulation loop, gathers statistics, and finalizes MPI.
 */

static void print_usage(const char *prog_name) {
    fprintf(stderr,
        "Usage: %s -n <rows> -m <cols> -i <iterations> [-s <seed>]\n"
        "  -n <rows>        Number of rows in the board (positive integer)\n"
        "  -m <cols>        Number of columns in the board (positive integer)\n"
        "  -i <iterations>  Number of simulation iterations (positive integer)\n"
        "  -s <seed>        Optional random seed (positive integer; default: time-based)\n",
        prog_name);
}

int main(int argc, char *argv[]) {
    int rows = 0, cols = 0, iterations = 0;
    int user_seed = 0;

    // 1. Manual parsing of argv[]
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0) {
            if (i + 1 < argc) {
                rows = atoi(argv[++i]);
            } else {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
        }
        else if (strcmp(argv[i], "-m") == 0) {
            if (i + 1 < argc) {
                cols = atoi(argv[++i]);
            } else {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
        }
        else if (strcmp(argv[i], "-i") == 0) {
            if (i + 1 < argc) {
                iterations = atoi(argv[++i]);
            } else {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
        }
        else if (strcmp(argv[i], "-s") == 0) {
            if (i + 1 < argc) {
                user_seed = atoi(argv[++i]);
            } else {
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
        }
        else {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (rows <= 0 || cols <= 0 || iterations <= 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    // 2. Initialize MPI
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // 3. Initialize random seed per rank
    unsigned int seed = init_seed(user_seed + rank);
    if (rank == 0) {
        printf("Using base seed: %u (rank0 uses %u)\n", user_seed, seed);
    }

    // 4. MASTER: create full random board
    char *full_board = NULL;
    if (rank == 0) {
        full_board = life_create(rows, cols, seed);
        if (!full_board) {
            fprintf(stderr, "Error: failed to allocate full board on MASTER.\n");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
    }

    // 5. Scatter the board row-based; each rank allocates local_buf
    char *local_buf = NULL;
    int local_rows = 0;
    mpi_scatter_board(full_board, rows, cols, &local_buf, &local_rows, MPI_COMM_WORLD);

    if (rank == 0) {
        free(full_board);
    }

    // 6. Allocate buffers for simulation (padded)
    int padded_rows = local_rows + 2;  // include top and bottom ghost rows
    size_t buf_size = (size_t)padded_rows * cols * sizeof(char);

    char *current = (char *)malloc(buf_size);
    char *next    = (char *)malloc(buf_size);
    if (!current || !next) {
        fprintf(stderr, "Error: failed to allocate local buffers on rank %d.\n", rank);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    // Copy scattered data into current[1..local_rows]
    // local_buf already has data starting at offset cols
    memcpy(current + cols, local_buf + cols, (size_t)local_rows * cols * sizeof(char));
    free(local_buf);

    // Initialize ghost rows to zero
    memset(current, 0, cols * sizeof(char));                              // top ghost
    memset(current + (local_rows + 1) * cols, 0, cols * sizeof(char));     // bottom ghost

    // 7. Simulation loop
    double start_time = get_time();

    for (int gen = 1; gen <= iterations; gen++) {
        // 7.1 Exchange ghost rows with neighbors
        mpi_exchange_ghosts(current, local_rows, cols, MPI_COMM_WORLD);

        // 7.2 Compute next generation for real rows
        life_step(current, next, local_rows, cols);

        // 7.3 Swap buffers
        char *tmp = current;
        current   = next;
        next      = tmp;

        // 7.4 Gather and print stats every iteration
        long local_alive  = life_count(current + cols, local_rows * cols);
        long global_alive = mpi_reduce_count(local_alive, MPI_COMM_WORLD);
        if (rank == 0) {
            double t_now = get_time();
            printf("[Gen %4d] Alive cells = %ld  Elapsed=%.4f s\n",
                   gen, global_alive, t_now - start_time);
        }
    }

    double total_time = get_time() - start_time;

    // 8. Final statistics (MASTER)
    if (rank == 0) {
        printf("Simulation complete: %d generations on a %dx%d board across %d ranks.\n",
               iterations, rows, cols, size);
        printf("Total elapsed time: %.4f s  Avg time/gen: %.6f s\n",
               total_time, total_time / iterations);
    }

    // 9. Cleanup local buffers and finalize
    free(current);
    free(next);

    MPI_Finalize();
    return 0;
}
