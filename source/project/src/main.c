//   ____    ____          _                      
//  |_   \  /   _|        (_)                     
//    |   \/   |   ,--.   __   _ .--.      .---.  
//    | |\  /| |  `'_\ : [  | [ `.-. |    / /'`\] 
//   _| |_\/_| |_ // | |, | |  | | | |  _ | \__.  
//  |_____||_____|\'-;__/[___][___||__](_)'.___.' 
//                                                

/**
 * @file main.c
 * @brief Entry point for MPI Game of Life simulation.
 *
 * parses command line arguments, initializes MPI, deploys board,
 * executes simulation loop with early-exit on:
 * 
 * 1) bit-to-bit steady state
 * 2) zero population
 * 3) cell count lives unchanged for K consecutive generations.
 * 
 * collects statistics and finalizes MPI.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include "life.h"
#include "mpix.h"
#include "utils.h"

/* ********************************************************************************************* */

static void print_usage(const char *prog_name);
static int parse_args(int argc, char *argv[], int *rows, int *cols, int *epochs, int *user_seed);

/* ********************************************************************************************* */

/**
 * @brief Full parse command-line arguments for rows, cols, epochs, and optional seed.
 *
 * Expects the following usage:
 *   -n <rows>        Number of rows in the board (positive integer)
 *   -m <cols>        Number of columns in the board (positive integer)
 *   -e <epochs>      Number of simulation epochs (positive integer)
 *   -s <seed>        Optional random seed (positive integer; default: time-based)
 *
 * If any required argument is missing or invalid, prints usage and returns non-zero.
 *
 * @param argc        Argument count from main().
 * @param argv        Argument vector from main().
 * @param rows        OUT: pointer to store parsed number of rows.
 * @param cols        OUT: pointer to store parsed number of columns.
 * @param epochs      OUT: pointer to store parsed number of epochs.
 * @param user_seed   OUT: pointer to store parsed seed (0 if none provided).
 * @return            0 on successful parse; non-zero on failure.
 */
static int parse_args(int argc, char *argv[],
                      int *rows, int *cols,
                      int *epochs, int *user_seed) {
    
    *rows = *cols = *epochs = *user_seed = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            *rows = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            *cols = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
            *epochs = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            *user_seed = atoi(argv[++i]);
        } else {
            print_usage(argv[0]);
            return -1;
        }
    }

    if (*rows <= 0 || *cols <= 0 || *epochs <= 0) {
        print_usage(argv[0]);
        return -1;
    }

    return 0;
}

/**
 * @brief Print program usage instructions to stderr.
 *
 * Displays the required command-line options and their descriptions:
 *   - -n <rows>       Number of rows in the board (positive integer)
 *   - -m <cols>       Number of columns in the board (positive integer)
 *   - -e <epochs>     Number of simulation epochs (positive integer)
 *   - -s <seed>       Optional random seed (positive integer; default: time-based)
 *
 * @param prog_name  Name of the executable (used to format the usage string)
 */
static void print_usage(const char *prog_name) {
    fprintf(stderr,
        "Usage: %s -n <rows> -m <cols> -e <epochs> [-s <seed>]\n"
        "  -n <rows>        Number of rows in the board (positive integer)\n"
        "  -m <cols>        Number of columns in the board (positive integer)\n"
        "  -i <epochs>      Number of simulation epochs (positive integer)\n"
        "  -s <seed>        Optional random seed (positive integer; default: time-based)\n",
        prog_name);
}

/* ********************************************************************************************* */

int main(int argc, char *argv[]) {
    // 1. Initialize MPI environment
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);  // get this process’s rank
    MPI_Comm_size(MPI_COMM_WORLD, &size);  // get total number of ranks

    // 2. Initialize command-line arguments
    int rows = 0, cols = 0, epochs = 0;
    int user_seed = 0;

    // 3. Parse command-line arguments
    //    and share them to the others processes
    if (rank == 0) {
        if (parse_args(argc, argv, &rows, &cols, &epochs, &user_seed) != 0) {
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
    }

    // Broadcast parsed values to everyone
    MPI_Bcast(&rows,      1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&cols,      1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&epochs,    1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&user_seed, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // 4. Initialize random seed separately on each rank
    //    add the rank to user_seed so that each rank has a different seed
    unsigned int seed = init_seed(user_seed + rank);

    if (rank == 0) {
        // Only the MASTER prints the base user seed and the actual seed used
        printf("Using base seed: %d (rank 0 uses %u)\n", user_seed, seed);
    }

    // 5. MASTER allocates and initializes the full board (rows × cols)
    char *full_board = NULL;
    if (rank == 0) {
        full_board = life_create(rows, cols, seed);
        if (!full_board) {
            fprintf(stderr, "Error: failed to allocate full board on MASTER.\n");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
    }

    // 6. Scatter the board row-wise so each rank receives its chunk
    //    local_buf will point to a padded buffer of size (local_rows + 2) × cols
    char *local_buf = NULL;
    int local_rows = 0;
    mpi_scatter_board(full_board, rows, cols, &local_buf, &local_rows, MPI_COMM_WORLD);

    // Once scattered, MASTER can free the full_board
    if (rank == 0) {
        free(full_board);
    }

    // 7. Allocate two local padded buffers: current and next, each of size (local_rows + 2) × cols
    int padded_rows = local_rows + 2;
    size_t buf_elems = (size_t)padded_rows * cols;
    char *current = malloc(buf_elems * sizeof(char));
    char *next    = malloc(buf_elems * sizeof(char));
    if (!current || !next) {
        fprintf(stderr, "Error: failed to allocate local buffers on rank %d.\n", rank);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    // 8. Copy scattered real rows into current[1 .. local_rows], leaving ghost rows at indices 0 and local_rows+1
    //    local_buf itself already has the real rows starting at index cols, so we offset by cols
    memcpy(current + cols, local_buf + cols, (size_t)local_rows * cols * sizeof(char));
    free(local_buf);

    // Zero out ghost rows:
    memset(current, 0, cols * sizeof(char));                            // top ghost row
    memset(current + (local_rows + 1) * cols, 0, cols * sizeof(char));  // bottom ghost row

    // 9. Begin simulation loop with early-exit conditions:
    //    - Zero population
    //    - Steady state (bitwise equality)
    //    - Alive count unchanged for STABLE_THRESHOLD generations
    
    int stable_count = 0;               // current stable count
    long prev_global_alive = -1;        // no previous alive count yet     
    const int STABLE_THRESHOLD = 10;    // exit if the same alive count repeats 10 times
    
    double start_time = get_time();

    for (int gen = 1; gen <= epochs; gen++) {
        // 9.1 Exchange ghost rows with neighbor ranks
        mpi_exchange_ghosts(current, local_rows, cols, MPI_COMM_WORLD);

        // 9.2 Compute next generation into 'next'
        life_step(current, next, local_rows, cols);

        // 9.3 Early-exit: check for steady state (no bit changes)
        if (mpi_check_steady_state(current, next, local_rows, cols, MPI_COMM_WORLD)) {
            // If steady, count alive cells in the new generation and print then exit
            long local_alive  = life_count(next + cols, local_rows * cols);
            long global_alive = mpi_reduce_count(local_alive, MPI_COMM_WORLD);
            if (rank == 0) {
                printf("Reached steady state at generation %d with %ld alive cells, exiting early.\n",
                       gen, global_alive);
            }
            break;
        }

        // 9.4 Swap buffers: current ← next, next ← current
        char *tmp = current;
        current   = next;
        next      = tmp;

        // 9.5 Gather alive-cell counts
        long local_alive  = life_count(current + cols, local_rows * cols);
        long global_alive = mpi_reduce_count(local_alive, MPI_COMM_WORLD);

        // 9.6 Early-exit: check for zero population
        if (mpi_check_zero_population(current, local_rows, cols, MPI_COMM_WORLD)) {
            if (rank == 0) {
                printf("All cells are dead at generation %d, exiting early.\n", gen);
            }
            break;
        }

        // 9.7 Early-exit: alive count unchanged for STABLE_THRESHOLD generations
        if (rank == 0) {
            if (prev_global_alive == global_alive) {
                stable_count++;
            } else {
                stable_count = 0;
            }
            prev_global_alive = global_alive;
        }

        // Broadcast updated stable_count from MASTER to all ranks
        MPI_Bcast(&stable_count, 1, MPI_INT, 0, MPI_COMM_WORLD);

        if (stable_count >= STABLE_THRESHOLD) {
            if (rank == 0) {
                printf("Alive count stayed at %ld for %d consecutive generations (gen %d), exiting early.\n",
                       prev_global_alive, STABLE_THRESHOLD, gen);
            }
            break;
        }

        // 9.8 Print per-generation statistics on MASTER
        if (rank == 0) {
            double t_now = get_time();
            printf("[Gen %4d] Alive cells = %ld  Elapsed = %.4f s\n",
                   gen, global_alive, t_now - start_time);
        }
    }

    // 10. Final summary printed by MASTER
    if (rank == 0) {
        double total_time = get_time() - start_time;
        printf("Simulation complete on a %dx%d board across %d ranks.\n",
               rows, cols, size);
        printf("Total time: %.4f s  Avg time/gen: %.6f s\n",
               total_time, total_time / epochs);
    }

    // 11. Cleanup local buffers and finalize MPI
    free(current);
    free(next);

    MPI_Finalize();
    return 0;
}

/* ********************************************************************************************* */
