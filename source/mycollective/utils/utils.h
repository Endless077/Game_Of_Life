#ifndef UTILS_H
#define UTILS_H

#include "../mycollective.h"

/* ********************************************************************************************* */

// Utils
void fill_array(void *data, int length, MPI_Datatype datatype);
int validate_input(int argc, char *argv[], int rank);

// MPI Get Datatype
MPI_Datatype get_mpi_datatype(const char *type_str);

// Reduce Operations
int min_op(int a, int b);
int max_op(int a, int b);

/* ********************************************************************************************* */

// Performance
void perform_broadcast(void *data, int length,  MPI_Datatype datatype, int rank, const char *operation_type);
void perform_scatter(void *data, int length, MPI_Datatype datatype, int rank, int size, const char *operation_type);
void perform_gather(void *data, int length, MPI_Datatype datatype, int rank, int size, const char *operation_type);
void perform_reduce(void *data, int length, MPI_Datatype datatype, int rank, int size, const char *operation_type);

/* ********************************************************************************************* */

#endif
