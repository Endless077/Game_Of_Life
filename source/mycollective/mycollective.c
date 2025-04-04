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

// Non-Blocking Broadcast
void NBbroadcast(void *data, int length, MPI_Datatype datatype, int root, MPI_Comm comm) {
    // Initialization
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    // Allocate arrays for requests and statuses
    MPI_Request *requests = malloc(size * sizeof(MPI_Request));
    MPI_Status *statuses = malloc(size * sizeof(MPI_Status));

    for (int i = 0; i < size; i++) {
        requests[i] = MPI_REQUEST_NULL;
    }

    // Root sends to all other processes
    if (rank == root) {
        for (int i = 0; i < size; i++) {
            if (i != root) {
                MPI_Isend(data, length, datatype, i, 0, comm, &requests[i]);
            }
        }
    } else {
        // Non-root receives from root
        MPI_Irecv(data, length, datatype, root, 0, comm, &requests[rank]);
    }

    // Wait for all sends/receives to complete
    MPI_Waitall(size, requests, statuses);

    free(requests);
    free(statuses);
}

// Non-Blocking Scatter
void NBscatter(void *sendbuf, void **recvbuf, int length, MPI_Datatype datatype, int root, MPI_Comm comm) {
    // Initialization
    int rank, size, typesize;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
    MPI_Type_size(datatype, &typesize);

    // Compute send counts and displacements
    int *sendcounts = malloc(size * sizeof(int));
    int *displs = malloc(size * sizeof(int));
    int base_count = length / size;
    int remainder = length % size;

    int offset = 0;
    for (int i = 0; i < size; i++) {
        sendcounts[i] = base_count + (i < remainder ? 1 : 0);
        displs[i] = offset;
        offset += sendcounts[i];
    }

    // Allocate local receive buffer
    int recv_count = sendcounts[rank];
    *recvbuf = malloc(recv_count * typesize);

    MPI_Request *requests = malloc((size - 1) * sizeof(MPI_Request));
    MPI_Status *statuses = malloc((size - 1) * sizeof(MPI_Status));

    // Root sends chunks or copies locally
    if (rank == root) {
        int req_idx = 0;
        for (int i = 0; i < size; i++) {
            if (i == root) {
                memcpy(*recvbuf, (char *)sendbuf + displs[i] * typesize, recv_count * typesize);
            } else {
                MPI_Isend((char *)sendbuf + displs[i] * typesize, sendcounts[i], datatype, i, 0, comm, &requests[req_idx++]);
            }
        }

        MPI_Waitall(size - 1, requests, statuses);
    } else {
        // Non-root receives its chunk
        MPI_Request req;
        MPI_Status status;
        MPI_Irecv(*recvbuf, recv_count, datatype, root, 0, comm, &req);
        MPI_Wait(&req, &status);
    }

    free(displs);
    free(sendcounts);
    free(requests);
    free(statuses);
}

// Non-Blocking Gather
void NBgather(void *sendbuf, void *recvbuf, int length, MPI_Datatype datatype, int root, MPI_Comm comm) {
    // Initialization
    int rank, size, typesize;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
    MPI_Type_size(datatype, &typesize);

    // Calculate local data size
    int base_count = length / size;
    int extra = length % size;
    int local_size = base_count + (rank < extra ? 1 : 0);

    int *recvcounts = NULL;
    int *displs = NULL;

    // Root sets up counts and displacements
    if (rank == root) {
        recvcounts = malloc(size * sizeof(int));
        displs = malloc(size * sizeof(int));

        int offset = 0;
        for (int i = 0; i < size; i++) {
            recvcounts[i] = base_count + (i < extra ? 1 : 0);
            displs[i] = offset;
            offset += recvcounts[i];
        }
    }

    MPI_Request *requests = malloc(size * sizeof(MPI_Request));
    MPI_Status *statuses = malloc(size * sizeof(MPI_Status));

    for (int i = 0; i < size; i++) {
        requests[i] = MPI_REQUEST_NULL;
    }

    // Non-root sends local data to root
    if (rank != root) {
        MPI_Isend(sendbuf, local_size, datatype, root, 0, comm, &requests[rank]);
    } else {
        // Root receives data from all and copies its own
        for (int i = 0; i < size; i++) {
            if (i == root) {
                memcpy((char *)recvbuf + displs[i] * typesize, sendbuf, local_size * typesize);
            } else {
                MPI_Irecv((char *)recvbuf + displs[i] * typesize, recvcounts[i], datatype, i, 0, comm, &requests[i]);
            }
        }
    }

    MPI_Waitall(size, requests, statuses);

    if (rank == root) {
        free(recvcounts);
        free(displs);
    }

    free(requests);
    free(statuses);
}

/* ********************************************************************************************* */

// Blocking Broadcast
void Bbroadcast(void *data, int count, MPI_Datatype datatype, int root, MPI_Comm comm) {
    // Initialization
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

// Blocking Scatter
void Bscatter(void *sendbuf, void **recvbuf, int length, MPI_Datatype datatype, int root, MPI_Comm comm) {
    // Initialization
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

// Blocking Gather
void Bgather(void *sendbuf, void *recvbuf, int length, MPI_Datatype datatype, int root, MPI_Comm comm) {
    // Initialization
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

// Non-Blocking Reduce
void NBreduce(void *data, int length, MPI_Datatype datatype, int root, MPI_Comm comm, int (*op)(int, int)) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
    
    int chunk = length / size;
    int remainder = length % size;
    int local_size = chunk + (rank < remainder ? 1 : 0);
    
    int *local_data = malloc(local_size * sizeof(int));
    MPI_Request request;
    MPI_Status status;

    if (rank == 0) {
        // Scatter data to processes
        for (int i = 1; i < size; i++) {
            int count = chunk + (i < remainder ? 1 : 0);
            MPI_Isend((int *)data + i * chunk, count, datatype, i, 0, comm, &request);
        }
        memcpy(local_data, data, local_size * sizeof(int));
    } else {
        // Receive data
        MPI_Irecv(local_data, local_size, datatype, root, 0, comm, &request);
        MPI_Wait(&request, &status);
    }

    // Perform reduction
    int local_result = local_data[0];
    for (int i = 1; i < local_size; i++) {
        local_result = op(local_result, local_data[i]);
    }

    // Reduce results at root
    if (rank == 0) {
        int global_result = local_result;
        for (int i = 1; i < size; i++) {
            int temp_result;
            MPI_Irecv(&temp_result, 1, datatype, i, 0, comm, &request);
            MPI_Wait(&request, &status);
            global_result = op(global_result, temp_result);
        }
        printf("Non-blocking reduce result: %d\n", global_result);
    } else {
        MPI_Isend(&local_result, 1, datatype, root, 0, comm, &request);
    }

    free(local_data);
}

// Blocking Reduce
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
