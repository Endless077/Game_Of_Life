/*
--------------------------------
MPI Point-to-Point Communication
--------------------------------
Ring: Given P processes, the process with rank i sends an integer value to the process with rank i+1. Note that the communication pattern is circular and toroidal,
so the process with rank P-1 sends to the process with rank 0. The program execution involves 10 iterations. In each iteration, processes increment the value
received from the left neighbor by a pseudo-random integer between 0-100. A particular iteration ends when the value received by a process exceeds a certain
threshold S provided as input to the program. At the end of the 10 iterations, the program writes to standard output (rank 0) the average number of
communication rounds needed to converge using P and S. Note that random number generators should not be reinitialized between iterations. It is
recommended to initialize the generators using the rank value.

Note:
The rank values refer to the processes' indices obtained from the MPI_COMM_WORLD communicator
Develop an MPI program in C for the following problems.
*/

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#include <time.h>

#define TAG_FINISH 1
#define TAG_RUNNING 0
#define TAG_GATHER 99

/* ********************************************************************************************* */

int main(int argc, char *argv[]) {
    int rank, size, threshold;
    const int max_iterations = 10;

    // Initialization
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    double start, end;

    MPI_Barrier(MPI_COMM_WORLD);
    start = MPI_Wtime();

    //Check and parse input threshold argument
    if (argc != 2) {
        if (rank == 0)
            printf("Usage: %s <threshold>\n", argv[0]);
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    threshold = atoi(argv[1]);
    if (threshold <= 0) {
        if (rank == 0)
            printf("Threshold must be > 0\n");
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    //Initialize random seed using rank
    srand(time(NULL) + rank);

    int left = (rank - 1 + size) % size;
    int right = (rank + 1) % size;

    int total_sends, local_sends = 0;
    int value, iteration = 0;
    int converged = 0;

    //Rank 0 initializes the ring and starts value propagation
    if (rank == 0) {
        value = rand() % 100;
        printf("Initialization value: %d\n", value);

        if (value > threshold) {
            printf("[Rank 0] Already exceeds threshold. Exiting.\n");
        } else {
            while (iteration < max_iterations) {
                iteration++;

                int add = rand() % 100;
                value += add;

                printf("[Iter %d | Rank %d] adds %d → %d → to %d\n", iteration, rank, add, value, right);
                MPI_Send(&value, 1, MPI_INT, right, TAG_RUNNING, MPI_COMM_WORLD);
                local_sends++;

                MPI_Status status;
                MPI_Recv(&value, 1, MPI_INT, left, TAG_RUNNING, MPI_COMM_WORLD, &status);

                if (value > threshold) {
                    converged = 1;
                    printf("[Iter %d | Rank %d] received %d > %d → Done.\n", iteration, rank, value, threshold);

                    MPI_Send(&value, 1, MPI_INT, right, TAG_RUNNING, MPI_COMM_WORLD);
                    local_sends++;
                    break;
                } else {
                    printf("[Iter %d | Rank %d] received %d → continue.\n", iteration, rank, value);
                }
            }

            //Handle case when no process caused convergence
            if (!converged) {
                printf("[Iter %d | Rank %d] Max iterations reached. Sending final value to close ring.\n", iteration, rank);
                MPI_Send(&value, 1, MPI_INT, right, TAG_RUNNING, MPI_COMM_WORLD);
                local_sends++;

                MPI_Status status;
                MPI_Recv(&value, 1, MPI_INT, left, TAG_RUNNING, MPI_COMM_WORLD, &status);
                printf("[Iter %d | Rank %d] Final message received back. Exiting cleanly.\n", iteration, rank);
            }
        }

        //Send termination signal through the ring
        MPI_Send(NULL, 0, MPI_INT, right, TAG_FINISH, MPI_COMM_WORLD);

    } else {
        //Other ranks participate in the ring until a stop condition is reached
        while (1) {
            MPI_Status status;
            MPI_Recv(&value, 1, MPI_INT, left, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            if (status.MPI_TAG == TAG_FINISH) {
                MPI_Send(NULL, 0, MPI_INT, right, TAG_FINISH, MPI_COMM_WORLD);
                break;
            }

            iteration++;

            if (value > threshold) {
                if (rank == 0) {
                    converged = 1;
                    printf("[Iter %d | Rank %d] received %d > %d → Done.\n", iteration, rank, value, threshold);
                } else {
                    printf("[Iter %d | Rank %d] received %d > %d → pass & exit\n", iteration, rank, value, threshold);
                }

                MPI_Send(&value, 1, MPI_INT, right, TAG_RUNNING, MPI_COMM_WORLD);
                local_sends++;
                break;
            } else {
                int add = rand() % 100;
                value += add;

                printf("[Iter %d | Rank %d] adds %d → %d → to %d\n", iteration, rank, add, value, right);
                MPI_Send(&value, 1, MPI_INT, right, TAG_RUNNING, MPI_COMM_WORLD);
                local_sends++;
            }
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);

    
    //Each process sends its local message count to rank 0
    if (rank != 0) {
        MPI_Send(&local_sends, 1, MPI_INT, 0, TAG_GATHER, MPI_COMM_WORLD);
    } else {
        total_sends = local_sends;

        int tmp;
        for (int i = 1; i < size; i++) {
            MPI_Status status;
            MPI_Recv(&tmp, 1, MPI_INT, i, TAG_GATHER, MPI_COMM_WORLD, &status);
            total_sends += tmp;
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    end = MPI_Wtime();
    MPI_Finalize();
    
    //Final reporting of execution metrics by rank 0
    if (rank == 0) {
        printf("\nExecution time (ms): %.3f\n", (end - start) * 1000);
        printf("Total messages exchanged: %d\n", total_sends);
        printf("Communication rounds: %d\n", iteration);
        printf("Convergence: %s\n", converged ? "YES" : "NO");
    }

    return 0;
}

/* ********************************************************************************************* */
