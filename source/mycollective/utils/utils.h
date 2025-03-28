#ifndef UTILS_H
#define UTILS_H

#include "../mycollective.h"

void fill_array(void *data, int N, MPI_Datatype datatype);
int validate_input(int argc, char *argv[], int rank);
MPI_Datatype get_mpi_datatype(const char *type_str);

void perform_broadcast(void *data, int N,  MPI_Datatype datatype, int rank, const char *operation_type);
void perform_scatter(void *data, int N, MPI_Datatype datatype, int rank, int size, const char *operation_type);
void perform_gather(void *data, int N, MPI_Datatype datatype, int rank, int size, const char *operation_type);

#endif
