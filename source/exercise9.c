/*
----------------------------
MPI Collective communication
----------------------------
This program distributes an array A of N integers across P MPI processes.
Each element A[i] is updated based on its neighbors:
    A[i] = A[i-1] + A[i] + A[i+1]
    with wrap-around logic for A[0] and A[N-1]
*/

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* *********************************************************************************************** */

// Initialize array with random integers
void initialize_array(int *array, int N) {
    srand(time(NULL));
    for (int i = 0; i < N; i++) {
        array[i] = rand() % 100;
    }
}

// Print array values
void print_array(int *array, int N) {
    for (int i = 0; i < N; i++) {
        printf("%d ", array[i]);
    }
    printf("\n\n");
}

// Update segment values with ghost cells
void update_segment(int *local_A, int local_N, int left_ghost, int right_ghost, int rank, int size) {
    int *tmp = (int *)malloc(local_N * sizeof(int));

    if (local_N == 1) {
        tmp[0] = left_ghost + local_A[0] + right_ghost;
    } else {
        for (int i = 0; i < local_N; i++) {
            if (i == 0) {
                tmp[i] = left_ghost + local_A[i] + local_A[i + 1];
            } else if (i == local_N - 1) {
                tmp[i] = local_A[i - 1] + local_A[i] + right_ghost;
            } else {
                tmp[i] = local_A[i - 1] + local_A[i] + local_A[i + 1];
            }
        }
    }

    for (int i = 0; i < local_N; i++) {
        local_A[i] = tmp[i];
    }

    free(tmp);
}

int main(int argc, char *argv[]) {
    int rank, size, N;
    int *A = NULL;
    double start, end;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0) {
        if (argc != 2) {
            printf("Usage: %s <N>\n", argv[0]);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        N = atoi(argv[1]);
        if (N < size) {
            printf("Error: N must be >= number of processes.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    start = MPI_Wtime();

    // Broadcast N to all processes
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Setup for scatter
    int *sendcounts = malloc(size * sizeof(int));
    int *displs = malloc(size * sizeof(int));
    int base = N / size;
    int rem = N % size;
    int offset = 0;
    for (int i = 0; i < size; i++) {
        sendcounts[i] = base + (i < rem ? 1 : 0);
        displs[i] = offset;
        offset += sendcounts[i];
    }

    int local_N = sendcounts[rank];
    int *local_A = malloc(local_N * sizeof(int));

    if (rank == 0) {
        A = malloc(N * sizeof(int));
        initialize_array(A, N);
        printf("Initialization Array:\n");
        print_array(A, N);
    }

    // Scatter the array
    MPI_Scatterv(A, sendcounts, displs, MPI_INT, local_A, local_N, MPI_INT, 0, MPI_COMM_WORLD);

    // Debug: print received array
    printf("Process %d received local array:", rank);
    for (int i = 0; i < local_N; i++) {
        printf(" %d", local_A[i]);
    }
    printf("\n");

    // Determine neighbors in ring
    int left = (rank - 1 + size) % size;
    int right = (rank + 1) % size;

    int left_ghost = 0, right_ghost = 0;

    // Exchange ghost cells
    MPI_Sendrecv(&local_A[0], 1, MPI_INT, left, 0,
                 &right_ghost, 1, MPI_INT, right, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    MPI_Sendrecv(&local_A[local_N - 1], 1, MPI_INT, right, 1,
                 &left_ghost, 1, MPI_INT, left, 1,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    printf("Process %d: left ghost = %d, right ghost = %d\n", rank, left_ghost, right_ghost);

    // Update local values
    update_segment(local_A, local_N, left_ghost, right_ghost, rank, size);

    // Debug: print updated local array
    printf("Process %d updated local array:", rank);
    for (int i = 0; i < local_N; i++) {
        printf(" %d", local_A[i]);
    }
    printf("\n");

    // Gather all updated segments
    MPI_Gatherv(local_A, local_N, MPI_INT, A, sendcounts, displs, MPI_INT, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    end = MPI_Wtime();
    MPI_Finalize();

    if (rank == 0) {
        printf("\nFinal array:\n");
        print_array(A, N);
        printf("Execution time (ms): %.3f\n", (end - start) * 1000);
        free(A);
    }

    free(displs);
    free(local_A);
    free(sendcounts);

    return 0;
}

/* ********************************************************************************************* */
