/*
--------------------------------
MPI Point-to-Point Communication
--------------------------------
Exchange of an integer value between two MPI processes.

Note:
The rank values refer to the processes' indices obtained from the MPI_COMM_WORLD communicator.
Develop an MPI program in C for the following problems, using only the MPI_Send and MPI_Recv operations.
*/

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#include <time.h>

int main(int argc, char *argv[]) {
    int rank, size;
    int number;
    double start, end;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size < 2) {
        if (rank == 0)
            printf("This program requires at least 2 processes.\n");
        MPI_Finalize();
        return 0;
    }

    // unique seed per rank
    srand(time(NULL) + rank);

    MPI_Barrier(MPI_COMM_WORLD);
    start = MPI_Wtime();

    if (rank == 0) {
        number = rand() % 100;
        MPI_Send(&number, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
        printf("Process %d sent the number: %d\n", rank, number);
        fflush(stdout);
    } else if (rank == 1) {
        MPI_Status status;
        MPI_Recv(&number, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
        printf("Process %d received the number: %d\n", rank, number);
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
