/*
----------------------------
MPI Collective communication
----------------------------
Develop an MPI program that, given a matrix of size NxM and P processes, calculates the maximum for each row of the
matrix using P processes fairly. Upon completion, the master process writes the maximum of each row to standard
output.

Repeat the previous point by calculating the minimum for each column.

Combine all the previous points into a single program.

Note:
The rank values refer to the processes' indices obtained from the MPI_COMM_WORLD communicator
Develop an MPI program in C for the following problems.
*/

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

// Initialize matrix with random integers
void initialize_matrix(int *matrix, int rows, int cols) {
    srand(time(NULL));
    for (int i = 0; i < rows * cols; i++) {
        matrix[i] = rand() % 100;
    }
}

// Print matrix (row-major)
void print_matrix(int *matrix, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%3d ", matrix[i * cols + j]);
        }
        printf("\n");
    }
}

int main(int argc, char *argv[]) {
    int rank, size, N, M;
    int *matrix = NULL;
    double start, end;

    // Initialization
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    MPI_Barrier(MPI_COMM_WORLD);
    start = MPI_Wtime();

    if (rank == 0) {
        if (argc != 3) {
            printf("Usage: %s <rows=N> <cols=M>\n", argv[0]);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        N = atoi(argv[1]);
        M = atoi(argv[2]);

        if (N < size) {
            printf("Error: N must be >= number of processes.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        matrix = malloc(N * M * sizeof(int));
        initialize_matrix(matrix, N, M);
        printf("Initial matrix (%d x %d):\n", N, M);
        print_matrix(matrix, N, M);
        printf("\n");
    }

    // Broadcast dimensions
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&M, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Compute how many rows each process gets
    int base_rows = N / size;
    int remainder = N % size;
    int local_rows = base_rows + (rank < remainder ? 1 : 0);

    int *sendcounts = malloc(size * sizeof(int));
    int *displs = malloc(size * sizeof(int));
    int offset = 0;
    for (int i = 0; i < size; i++) {
        sendcounts[i] = (N / size + (i < remainder ? 1 : 0)) * M;
        displs[i] = offset;
        offset += sendcounts[i];
    }

    int *local_matrix = malloc(local_rows * M * sizeof(int));

    MPI_Scatterv(matrix, sendcounts, displs, MPI_INT,
                 local_matrix, local_rows * M, MPI_INT,
                 0, MPI_COMM_WORLD);

    // Compute max per row
    int *local_row_max = malloc(local_rows * sizeof(int));
    for (int i = 0; i < local_rows; i++) {
        int max = local_matrix[i * M];
        for (int j = 1; j < M; j++) {
            max = MAX(max, local_matrix[i * M + j]);
        }
        local_row_max[i] = max;
    }

    int *row_max_result = NULL;
    if (rank == 0) row_max_result = malloc(N * sizeof(int));

    int *recvcounts_rows = malloc(size * sizeof(int));
    int *displs_rows = malloc(size * sizeof(int));
    offset = 0;
    for (int i = 0; i < size; i++) {
        recvcounts_rows[i] = sendcounts[i] / M;
        displs_rows[i] = offset;
        offset += recvcounts_rows[i];
    }

    MPI_Gatherv(local_row_max, local_rows, MPI_INT,
                row_max_result, recvcounts_rows, displs_rows, MPI_INT,
                0, MPI_COMM_WORLD);

    // Compute min per column
    int *local_col_min = malloc(M * sizeof(int));
    for (int j = 0; j < M; j++) {
        local_col_min[j] = local_matrix[j];
        for (int i = 1; i < local_rows; i++) {
            local_col_min[j] = MIN(local_col_min[j], local_matrix[i * M + j]);
        }
    }

    int *global_col_min = NULL;
    if (rank == 0) global_col_min = malloc(M * sizeof(int));

    MPI_Reduce(local_col_min, global_col_min, M, MPI_INT, MPI_MIN, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    end = MPI_Wtime();
    MPI_Finalize();

    if (rank == 0) {
        printf("Maximum of each row:\n");
        for (int i = 0; i < N; i++) {
            printf("Row %d: %d\n", i, row_max_result[i]);
        }

        printf("\nMinimum of each column:\n");
        for (int j = 0; j < M; j++) {
            printf("Col %d: %d\n", j, global_col_min[j]);
        }

        printf("\nExecution time (ms) = %.3f\n", (end - start) * 1000);

        free(matrix);
        free(row_max_result);
        free(global_col_min);
    }

    free(local_matrix);
    free(local_row_max);
    free(local_col_min);

    free(displs);
    free(sendcounts);
    free(displs_rows);
    free(recvcounts_rows);

    return 0;
}
