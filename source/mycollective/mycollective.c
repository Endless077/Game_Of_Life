//                                       __   __                _    _                 
//                                      [  | [  |              / |_ (_)                
//   _ .--..--.    _   __  .---.   .--.  | |  | | .---.  .---.`| |-'__  _   __  .---.  
//  [ `.-. .-. |  [ \ [  ]/ /'`\]/ .'`\ \| |  | |/ /__\\/ /'`\]| | [  |[ \ [  ]/ /__\\ 
//   | | | | | |   \ '/ / | \__. | \__. || |  | || \__.,| \__. | |, | | \ \/ / | \__., 
//  [___||__||__][\_:  /  '.___.' '.__.'[___][___]'.__.''.___.'\__/[___] \__/   '.__.' 
//                \__.'                                                                
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mycollective.h"

/* ********************************************************************************************* */

// Function Blocking Broadcast
void Bbroadcast(void *data, int count, MPI_Datatype datatype, int root, MPI_Comm comm) {
    // Initialize
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    if (rank == root) {
        // Send the array to all other processes
        for (int i = 0; i < size; i++) {
            if (i != root)
                MPI_Send(data, count, datatype, i, 0, comm);
        }
    } else {
        // All processes receive the array from the root
        MPI_Status status;
        MPI_Recv(data, count, datatype, root, 0, comm, &status);
    }
}

// Function Blocking Scatter
void Bscatter(void *sendbuf, void **recvbuf, int length, MPI_Datatype datatype, int root, MPI_Comm comm) {
    // Initialize
    int rank, size, typesize;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
    MPI_Type_size(datatype, &typesize);

    // Calculate partitions
    int extra = length % size;
    int base_count = length / size;

    // Allocate send counts and displacements buffers
    int *sendcounts = malloc(size * sizeof(int));
    int *displs = malloc(size * sizeof(int));

    // Compute displacements based on sendcounts
    int offset = 0;
    for (int i = 0; i < size; i++) {
        sendcounts[i] = base_count + (i < extra ? 1 : 0);
        displs[i] = offset;
        offset += sendcounts[i];
    }

    // Determine how many elements this process receives
    int recv_count = 0;
    if (sendcounts) recv_count = sendcounts[rank];
    *recvbuf = malloc(recv_count * typesize);

    if (rank == root) {
        // Send each process its corresponding portion of the array
        for (int i = 0; i < size; i++) {
            if (i == root) {
                // Copy the root's portion of data into its receive buffer
                memcpy(*recvbuf, (char*)sendbuf + displs[i] * typesize, recv_count * typesize);
            } else {
                MPI_Send((char*)sendbuf + displs[i] * typesize, sendcounts[i], datatype, i, TAG_SCATTER, comm);
            }
        }
    } else {
        MPI_Status status;
        MPI_Recv(*recvbuf, recv_count, datatype, root, TAG_SCATTER, comm, &status);
    }

    free(sendcounts);
    free(displs);
}

// Function Blocking Gather
void Bgather(void *sendbuf, void *recvbuf, int length, MPI_Datatype datatype, int root, MPI_Comm comm) {
    // Initialize
    int rank, size, typesize;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
    MPI_Type_size(datatype, &typesize);

    // Calculate partitions
    int extra = length % size;
    int base_count = length / size;

    // Allocate receive counts and displacements buffers
    int *recvcounts = malloc(size * sizeof(int));
    int *displs = malloc(size * sizeof(int));

    int offset = 0;
    for (int i = 0; i < size; i++) {
        recvcounts[i] = base_count + (i < extra ? 1 : 0);
        displs[i] = offset;
        offset += recvcounts[i];
    }

    // Determine how many elements this process sends
    int send_count = 0;
    if (recvcounts) send_count = recvcounts[rank];

    if (rank != root) {
        MPI_Send(sendbuf, send_count, datatype, root, TAG_GATHER, comm);
    } else {
        // Root copies its own data into the correct place in the final buffer
        memcpy((char*)recvbuf + displs[rank] * typesize, sendbuf, send_count * typesize);
        for (int i = 0; i < size; i++) {
            if (i != root) {
                MPI_Status status;
                MPI_Recv((char*)recvbuf + displs[i] * typesize, recvcounts[i], datatype, i, TAG_GATHER, comm, &status);
            }
        }
    }

    free(recvcounts);
    free(displs);
}

/* ********************************************************************************************* */

// Funzione generica per NBreduce (non-blocking)
void NBreduce(void *data, int N, MPI_Datatype datatype, int root, MPI_Comm comm, int (*function)(int, int)) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    int *local_data = NULL;
    int local_size = N / size + (rank < N % size ? 1 : 0);

    // Step 1: Non-blocking scatter
    // TODO: Non-blocking scatter

    // Step 2: Calcolo locale
    int local_result = local_data[0];
    for (int i = 1; i < local_size; i++) {
        local_result = function(local_result, local_data[i]);
    }

    free(local_data);

    // Step 3: Non-blocking gather
    int *gathered = NULL;
    if (rank == root) {
        gathered = malloc(size * sizeof(int));
    }

    // TODO: Non-Blocking Gather

    // Step 4: Riduzione finale sul root
    if (rank == root) {
        int final_result = gathered[0];
        for (int i = 1; i < size; i++) {
            final_result = function(final_result, gathered[i]);
        }

        printf("Reduce Result (Non-Blocking): %d\n", final_result);
        free(gathered);
    }
}

// Funzione generica per Breduce (blocking)
void Breduce(void *data, int N, MPI_Datatype datatype, int root, MPI_Comm comm, int (*function)(int, int)) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    int *local_data;

    // Step 1: Scatter
    Bscatter(data, (void **)&local_data, N, datatype, 0, MPI_COMM_WORLD);

    // Step 2: Local computation
    int local_result = local_data[0];
    for (int i = 1; i < (N / size + (rank < N % size ? 1 : 0)); i++) {
        local_result = function(local_result, local_data[i]);
    }

    free(local_data);

    // Step 3: Gather local results at root
    int *gathered = NULL;
    if (rank == 0) {
        gathered = malloc(size * sizeof(int));
    }

    Bgather(&local_result, gathered, size, MPI_INT, root, comm);

    // Step 4: Root computes final result
    if (rank == 0) {
        int final_result = gathered[0];
        for (int i = 1; i < size; i++) {
            final_result = function(final_result, gathered[i]);
        }

        printf("Reduce Result (blocking): %d\n", final_result);
        free(gathered);
    }
}

/* ********************************************************************************************* */
