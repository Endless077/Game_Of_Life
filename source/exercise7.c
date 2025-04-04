/*
------------------------------
MPI Non-blocking communication
------------------------------
Ring 2: Given P processors, develop an MPI program where each processor sends its rank to its right neighbor in the
order defined by MPI_COMM_WORLD and receives from the left neighbor. The communication of the rank should occur in a
non-blocking manner. Additionally, each process forwards the messages it receives from the left neighbor and sums all
the values, including its own, into a variable. The process terminates when each process has received the rank of all others
and writes the calculated sum to the standard output.

Note:
The rank values refer to the processes' indices obtained from the MPI_COMM_WORLD communicator
Develop an MPI program in C for the following problems.
*/

#include <mpi.h>
#include <stdio.h>

/* ********************************************************************************************* */

// Blocking Version
/*
------------------------------
MPI Blocking communication
------------------------------
Ring 2: Given P processors, develop an MPI program where each processor sends its rank to its right neighbor in the
order defined by MPI_COMM_WORLD and receives from the left neighbor. The communication of the rank should occur in a
blocking manner. Additionally, each process forwards the messages it receives from the left neighbor and sums all
the values, including its own, into a variable. The process terminates when each process has received the rank of all others
and writes the calculated sum to the standard output.
*/

#include <mpi.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    int rank, size;
    int send_value, recv_value;
    double start, end;

    int sum = 0;

    // Initialize MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int right = (rank + 1) % size;
    int left  = (rank - 1 + size) % size;

    send_value = rank;
    sum = rank;

    // Wait until all processes are initialized
    MPI_Barrier(MPI_COMM_WORLD);
    start = MPI_Wtime();

    for (int i = 0; i < size - 1; i++) {
        // Blocking send and receive using MPI_Sendrecv to avoid deadlock
        MPI_Sendrecv(&send_value, 1, MPI_INT, right, 0,
                     &recv_value, 1, MPI_INT, left, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        printf("Process %d: Received %d from %d at step %d\n", rank, recv_value, left, i + 1);

        sum += recv_value;
        send_value = recv_value;
    }

    printf("Process %d: Final sum = %d\n", rank, sum);

    MPI_Barrier(MPI_COMM_WORLD);
    end = MPI_Wtime();
    MPI_Finalize();

    if (rank == 0) {
        printf("\nExecution time (ms): %.3f\n", (end - start) * 1000);
    }

    return 0;
}

/* ********************************************************************************************* */
