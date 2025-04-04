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

// Non-Blocking Version
int main(int argc, char *argv[]) {
    int rank, size;
    int send_value, recv_value;
    double start, end;

    int sum = 0;

    // Initialize MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Wait until all processes are initialized
    MPI_Barrier(MPI_COMM_WORLD);
    start = MPI_Wtime();

    int right = (rank + 1) % size;
    int left  = (rank - 1 + size) % size;

    send_value = rank;
    sum = rank;

    MPI_Request send_req, recv_req;

    for (int i = 0; i < size - 1; i++) {
        // Non-blocking send to right neighbor
        MPI_Isend(&send_value, 1, MPI_INT, right, 0, MPI_COMM_WORLD, &send_req);
        // Non-blocking receive from left neighbor
        MPI_Irecv(&recv_value, 1, MPI_INT, left, 0, MPI_COMM_WORLD, &recv_req);

        // Wait for both send and receive to complete
        MPI_Wait(&send_req, MPI_STATUS_IGNORE);
        MPI_Wait(&recv_req, MPI_STATUS_IGNORE);

        printf("Process %d: Received %d from %d at step %d\n", rank, recv_value, left, i + 1);

        sum += recv_value;
        send_value = recv_value;
    }

    // Print the final sum for each process
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
