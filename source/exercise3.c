#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#include <time.h>

#define TAG_BROADCAST 0
#define TAG_SCATTER 1
#define TAG_GATHER 2

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

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int *data = NULL;
    int extra = N % size;
    int base_count = N / size;

    int *displs = (int*)malloc(size * sizeof(int));
    int *sendcounts = (int*)malloc(size * sizeof(int));

    // Initialize data at root
    if (rank == 0) {
        data = (int*) malloc(N * sizeof(int));
        srand(time(NULL));
        printf("Process %d generating random array:\n", rank);
        for (int i = 0; i < N; i++) {
            data[i] = rand() % 100;
            printf("%d ", data[i]);
        }
        printf("\n\n");
    }
    
    // Compute how many elements per process
    int offset = 0;
    for (int i = 0; i < size; i++) {
        sendcounts[i] = base_count + (i < extra ? 1 : 0);
        displs[i] = offset;
        offset += sendcounts[i];
    }

    int recv_count = sendcounts[rank];
    int *recv_buffer = (int*)malloc(recv_count * sizeof(int));

    MPI_Barrier(MPI_COMM_WORLD);
    start = MPI_Wtime();

    /*******************/
    /*     BROADCAST   */
    /*******************/
    if (rank == 0) {
        for (int dest = 1; dest < size; dest++) {
            MPI_Send(data, N, MPI_INT, dest, TAG_BROADCAST, MPI_COMM_WORLD);
        }
    } else {
        data = (int*) malloc(N * sizeof(int));
        MPI_Recv(data, N, MPI_INT, 0, TAG_BROADCAST, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    printf("Process %d received broadcasted array: ", rank);
    for (int i = 0; i < N; i++) {
        printf("%d ", data[i]);
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
                MPI_Send(&data[displs[dest]], sendcounts[dest], MPI_INT, dest, TAG_SCATTER, MPI_COMM_WORLD);
            }
        }
    } else {
        MPI_Recv(recv_buffer, recv_count, MPI_INT, 0, TAG_SCATTER, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    printf("Process %d received values from scatter: ", rank);
    for (int i = 0; i < recv_count; i++) {
        printf("%d ", recv_buffer[i]);
        recv_buffer[i] += 1;
    }
    printf("\n");
    fflush(stdout);

    /*******************/
    /*      GATHER     */
    /*******************/
    if (rank == 0) {
        int *gathered = (int*) malloc(N * sizeof(int));
        for (int i = 0; i < sendcounts[0]; i++)
            gathered[i] = recv_buffer[i];

        for (int src = 1; src < size; src++) {
            MPI_Recv(&gathered[displs[src]], sendcounts[src], MPI_INT, src, TAG_GATHER, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        printf("Process %d gathered values: ", rank);
        for (int i = 0; i < N; i++) {
            printf("%d ", gathered[i]);
        }
        printf("\n");
        fflush(stdout);
        free(gathered);
    } else {
        MPI_Send(recv_buffer, recv_count, MPI_INT, 0, TAG_GATHER, MPI_COMM_WORLD);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    end = MPI_Wtime();

    MPI_Finalize();

    if (rank == 0) {
        printf("Execution time (ms) = %f\n", (end - start) * 1000);
        free(data);
    }

    free(recv_buffer);
    free(sendcounts);
    free(displs);

    return 0;
}
