/*
--------------------------------
MPI Point-to-Point Communication
--------------------------------
Sending a string (read from stdin) from the process with rank 0 to the process with rank 1.

Note:
The rank values refer to the processes' indices obtained from the MPI_COMM_WORLD communicator.
Develop an MPI program in C for the following problems, using only the MPI_Send and MPI_Recv operations.
*/

#include <mpi.h>
#include <stdio.h>
#include <string.h>

#define MAX_LEN 256

int main(int argc, char *argv[]) {
    int rank, size;
    double start, end;
    char message[MAX_LEN];

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size < 2) {
        if (rank == 0)
            printf("This program requires at least 2 processes.\n");
        MPI_Finalize();
        return 0;
    }

    if (rank == 0) {
        printf("Enter a string to send to process 1: ");
        fgets(message, MAX_LEN, stdin);
        message[strcspn(message, "\n")] = 0;
        fflush(stdout);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    start = MPI_Wtime();

    if (rank == 0) {
        MPI_Send(message, strlen(message) + 1, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
        printf("Process %d sent: \"%s\"\n", rank, message);
        fflush(stdout);
    } else if (rank == 1) {
        MPI_Status status;
        MPI_Recv(message, MAX_LEN, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &status);
        printf("Process %d received: \"%s\"\n", rank, message);
        fflush(stdout);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    end = MPI_Wtime();
    MPI_Finalize();

    if (rank == 0) {
        printf("Execution time (ms) = %f\n", (end - start) * 1000);
    }

    return 0;
}
