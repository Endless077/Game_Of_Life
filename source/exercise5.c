/*
--------------------------------
MPI Point-to-Point Communication
--------------------------------
Develop the Reduce functionality in the mycollective library, capable of supporting the maximum and minimum operators of an array of integers.

Note:
The rank values refer to the processes' indices obtained from the MPI_COMM_WORLD communicator
Develop an MPI program in C for the following problems.
*/

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#include <limits.h>
#include <string.h>
#include <time.h>

/* ********************************************************************************************* */

// ----------------------
// Non-blocking version
// ----------------------
void nonblocking_min_max(int *data, int N, int rank, int size) {
    int chunk = N / size;
    int remainder = N % size;

    int *local_data = malloc((chunk + 1) * sizeof(int));
    int local_size;

    MPI_Request request;
    MPI_Status status;

    if (rank == 0) {
        // Step 1: Send data with MPI_Isend + MPI_Wait (robusto)
        for (int i = 1; i < size; i++) {
            int count = chunk + (i < remainder ? 1 : 0);
            int offset = i * chunk + (i < remainder ? i : remainder);
            MPI_Isend(&data[offset], count, MPI_INT, i, 0, MPI_COMM_WORLD, &request);
            MPI_Wait(&request, MPI_STATUS_IGNORE);
        }

        local_size = chunk + (0 < remainder ? 1 : 0);
        memcpy(local_data, data, local_size * sizeof(int));
    } else {
        // Step 2: Receive with MPI_Irecv + MPI_Wait
        local_size = chunk + (rank < remainder ? 1 : 0);
        MPI_Irecv(local_data, local_size, MPI_INT, 0, 0, MPI_COMM_WORLD, &request);
        MPI_Wait(&request, &status);
    }

    // Step 3: Compute local min and max
    int local_min = INT_MAX, local_max = INT_MIN;
    for (int i = 0; i < local_size; i++) {
        if (local_data[i] < local_min) local_min = local_data[i];
        if (local_data[i] > local_max) local_max = local_data[i];
    }

    // Step 4: Gather results
    if (rank == 0) {
        int global_min = local_min;
        int global_max = local_max;

        int *mins = malloc(size * sizeof(int));
        int *maxs = malloc(size * sizeof(int));
        MPI_Request *recv_reqs = malloc(2 * (size - 1) * sizeof(MPI_Request));
        MPI_Status *statuses = malloc(2 * (size - 1) * sizeof(MPI_Status));

        mins[0] = local_min;
        maxs[0] = local_max;

        // Non-blocking receives for mins and maxs
        for (int i = 1; i < size; i++) {
            MPI_Irecv(&mins[i], 1, MPI_INT, i, 1, MPI_COMM_WORLD, &recv_reqs[i - 1]);
            MPI_Irecv(&maxs[i], 1, MPI_INT, i, 2, MPI_COMM_WORLD, &recv_reqs[size - 1 + i - 1]);
        }

        MPI_Waitall(2 * (size - 1), recv_reqs, statuses);

        for (int i = 1; i < size; i++) {
            if (mins[i] < global_min) global_min = mins[i];
            if (maxs[i] > global_max) global_max = maxs[i];
        }

        printf("Non-blocking min: %d\n", global_min);
        printf("Non-blocking max: %d\n", global_max);

        free(mins);
        free(maxs);
        free(recv_reqs);
        free(statuses);
    } else {
        MPI_Request send_reqs[2];
        MPI_Status send_stats[2];

        MPI_Isend(&local_min, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &send_reqs[0]);
        MPI_Isend(&local_max, 1, MPI_INT, 0, 2, MPI_COMM_WORLD, &send_reqs[1]);
        MPI_Waitall(2, send_reqs, send_stats);
    }

    free(local_data);
}

// ----------------------
// Blocking version
// ----------------------
void blocking_min_max(int *data, int N, int rank, int size) {
    int chunk = N / size;
    int remainder = N % size;

    int *local_data = malloc((chunk + 1) * sizeof(int));
    int local_size;

    if (rank == 0) {
        // Step 1: Send data with MPI_Send
        for (int i = 1; i < size; i++) {
            int count = chunk + (i < remainder ? 1 : 0);
            MPI_Send(&data[i * chunk + (i < remainder ? i : remainder)], count, MPI_INT, i, 0, MPI_COMM_WORLD);
        }

        local_size = chunk + (0 < remainder ? 1 : 0);
        memcpy(local_data, data, local_size * sizeof(int));
    } else {
        // Step 2: Receive with MPI_Recv
        local_size = chunk + (rank < remainder ? 1 : 0);
        MPI_Recv(local_data, local_size, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    // Step 3: Get local Min/Max
    int local_min = INT_MAX, local_max = INT_MIN;
    for (int i = 0; i < local_size; i++) {
        if (local_data[i] < local_min) local_min = local_data[i];
        if (local_data[i] > local_max) local_max = local_data[i];
    }

    // Step 4: Gather all the locals Min/Max
    if (rank == 0) {
        int global_min = local_min;
        int global_max = local_max;
        for (int i = 1; i < size; i++) {
            int temp_min, temp_max;
            MPI_Recv(&temp_min, 1, MPI_INT, i, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&temp_max, 1, MPI_INT, i, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (temp_min < global_min) global_min = temp_min;
            if (temp_max > global_max) global_max = temp_max;
        }

        printf("Blocking min: %d\n", global_min);
        printf("Blocking max: %d\n", global_max);
    } else {
        MPI_Send(&local_min, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
        MPI_Send(&local_max, 1, MPI_INT, 0, 2, MPI_COMM_WORLD);
    }

    free(local_data);
}

/* ********************************************************************************************* */

int main(int argc, char *argv[]) {
    int rank, size;
    double start, end;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 3) {
        if (rank == 0)
            printf("Usage: %s <N> <blocking|non-blocking>\n", argv[0]);
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    int N = atoi(argv[1]);
    char *mode = argv[2];

    int *data = NULL;
    if (rank == 0) {
        data = malloc(N * sizeof(int));
        srand(time(NULL));
        printf("Initialized array:\n");
        for (int i = 0; i < N; i++) {
            data[i] = rand() % 1000;
            printf("%d ", data[i]);
        }
        printf("\n");
    }

    MPI_Barrier(MPI_COMM_WORLD);
    start = MPI_Wtime();

    if (strcmp(mode, "non-blocking") == 0) {
        nonblocking_min_max(data, N, rank, size);
    }else if (strcmp(mode, "blocking") == 0) {
        blocking_min_max(data, N, rank, size);
    } else {
        if (rank == 0) printf("Invalid mode.\n");
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    end = MPI_Wtime();
    MPI_Finalize();

    if (rank == 0) {
        printf("Execution time (ms): %.3f\n", (end - start) * 1000);
        free(data);
    }
    
    return 0;
}

/* ********************************************************************************************* */
