/*
----------------------------
MPI Collective communication
----------------------------
Broadcasting: Develop an MPI program utilizing P processors, where the process with rank 0 in MPI_COMM_WORLD
broadcasts an array of doubles to all other processes in MPI_COMM_WORLD.

Scattering: Develop an MPI program utilizing P processors, where the process with rank 0 in MPI_COMM_WORLD
scatters an array of doubles among all processes in MPI_COMM_WORLD.

Gathering: Develop an MPI program utilizing P processors, where the process with rank 0 in MPI_COMM_WORLD
gathers a set of double values distributed among all processes in MPI_COMM_WORLD.

Note:
The rank values refer to the processes' indices obtained from the MPI_COMM_WORLD communicator
Develop an MPI program in C for the following problems.
*/

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#include <time.h>

#define TAG_BROADCAST 0
#define TAG_SCATTER 1
#define TAG_GATHER 2

/* ********************************************************************************************* */

int main(int argc, char *argv[]) {
    int rank, size;
    double start, end;

    if (argc != 2) {
        if (argc < 2) printf("Usage: %s <N>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int N = atoi(argv[1]);
    if (N <= 0) {
        printf("N must be a positive integer.\n");
        return EXIT_FAILURE;
    }

    // Initialization
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    double *data = NULL;
    int extra = N % size;
    int base_count = N / size;

    int *displs = (int*)malloc(size * sizeof(int));
    int *sendcounts = (int*)malloc(size * sizeof(int));

    if (rank == 0) {
        data = (double*) malloc(N * sizeof(double));
        srand(time(NULL));
        printf("Initialization Array:\n", rank);
        for (int i = 0; i < N; i++) {
            data[i] = (rand() % 1000) / 10.0;
            printf("%.1f ", data[i]);
        }
        printf("\n\n");
    }

    // Compute how many elements each process will receive
    int offset = 0;
    for (int i = 0; i < size; i++) {
        sendcounts[i] = base_count + (i < extra ? 1 : 0);
        displs[i] = offset;
        offset += sendcounts[i];
    }

    int recv_count = sendcounts[rank];
    double *recv_buffer = (double*)malloc(recv_count * sizeof(double));

    MPI_Barrier(MPI_COMM_WORLD);
    start = MPI_Wtime();

    /*******************/
    /*     BROADCAST   */
    /*******************/
    if (rank == 0) {
        for (int dest = 1; dest < size; dest++) {
            MPI_Send(data, N, MPI_DOUBLE, dest, TAG_BROADCAST, MPI_COMM_WORLD);
        }
    } else {
        MPI_Status status;
        data = (double*) malloc(N * sizeof(double));
        MPI_Recv(data, N, MPI_DOUBLE, 0, TAG_BROADCAST, MPI_COMM_WORLD, &status);
    }

    printf("Process %d received broadcasted array: ", rank);
    for (int i = 0; i < N; i++) {
        printf("%.1f ", data[i]);
    }
    printf("\n");
    fflush(stdout);

    /*******************/
    /*      SCATTER    */
    /*******************/
    if (rank == 0) {
        for (int dest = 0; dest < size; dest++) {
            if (dest == 0) {
                for (int i = 0; i < sendcounts[0]; i++)
                    recv_buffer[i] = data[i];
            } else {
                MPI_Send(&data[displs[dest]], sendcounts[dest], MPI_DOUBLE, dest, TAG_SCATTER, MPI_COMM_WORLD);
            }
        }
    } else {
        MPI_Status status;
        MPI_Recv(recv_buffer, recv_count, MPI_DOUBLE, 0, TAG_SCATTER, MPI_COMM_WORLD, &status);
    }

    printf("Process %d received values from scatter: ", rank);
    for (int i = 0; i < recv_count; i++) {
        printf("%.1f ", recv_buffer[i]);
        recv_buffer[i] += 1.0; // Modify data
    }
    printf("\n");
    fflush(stdout);

    /*******************/
    /*      GATHER     */
    /*******************/
    if (rank == 0) {
        double *gathered = (double*) malloc(N * sizeof(double));
        for (int i = 0; i < sendcounts[0]; i++)
            gathered[i] = recv_buffer[i];

        for (int src = 1; src < size; src++) {
            MPI_Status status;
            MPI_Recv(&gathered[displs[src]], sendcounts[src], MPI_DOUBLE, src, TAG_GATHER, MPI_COMM_WORLD, &status);
        }

        printf("Process %d gathered values: ", rank);
        for (int i = 0; i < N; i++) {
            printf("%.1f ", gathered[i]);
        }
        printf("\n");
        free(gathered);
        fflush(stdout);
    } else {
        MPI_Send(recv_buffer, recv_count, MPI_DOUBLE, 0, TAG_GATHER, MPI_COMM_WORLD);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    end = MPI_Wtime();
    MPI_Finalize();

    if (rank == 0) {
        printf("\nExecution time (ms) = %.3f\n", (end - start) * 1000);
        free(data);
    }

    free(recv_buffer);
    free(sendcounts);
    free(displs);

    return 0;
}

/* ********************************************************************************************* */