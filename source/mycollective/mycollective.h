#ifndef MYCOLLECTIVE_H
#define MYCOLLECTIVE_H

#include <mpi.h>

#define TAG_GATHER 2
#define TAG_SCATTER 1
#define TAG_BROADCAST 0

/* ********************************************************************************************* */

void NBbroadcast(void *data, int count, MPI_Datatype datatype, int root, MPI_Comm comm);
void NBscatter(void *sendbuf, void **recvbuf, int N, MPI_Datatype datatype, int root, MPI_Comm comm);
void NBgather(void *sendbuf, void *recvbuf, int N, MPI_Datatype datatype, int root, MPI_Comm comm);

/* ********************************************************************************************* */

void Bbroadcast(void *data, int count, MPI_Datatype datatype, int root, MPI_Comm comm);
void Bscatter(void *sendbuf, void **recvbuf, int N, MPI_Datatype datatype, int root, MPI_Comm comm);
void Bgather(void *sendbuf, void *recvbuf, int N, MPI_Datatype datatype, int root, MPI_Comm comm);

/* ********************************************************************************************* */

void NBreduce(void *data, int N, MPI_Datatype datatype, int root, MPI_Comm comm, int (*function)(int, int));
void Breduce(void *data, int N, MPI_Datatype datatype, int root, MPI_Comm comm, int (*function)(int, int));

/* ********************************************************************************************* */

#endif
