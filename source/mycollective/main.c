//   ____    ____       _       _____  ____  _____  
//  |_   \  /   _|     / \     |_   _||_   \|_   _| 
//    |   \/   |      / _ \      | |    |   \ | |   
//    | |\  /| |     / ___ \     | |    | |\ \| |   
//   _| |_\/_| |_  _/ /   \ \_  _| |_  _| |_\   |_  
//  |_____||_____||____| |____||_____||_____|\____| 
//                                                  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/utils.h"

int main(int argc, char *argv[]) {
    // Initialize MPI environment
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Validate user input arguments
    if (!validate_input(argc, argv, rank)) {
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    // Parse command-line arguments
    int length = atoi(argv[1]);
    char *type = argv[2];
    char *operation = argv[3];
    char *operation_type = argv[4];

    // Determine MPI datatype from input string
    MPI_Datatype datatype = get_mpi_datatype(type);

    // Compute element size based on datatype
    int type_size;
    if (datatype == MPI_INT) {
        type_size = sizeof(int);
    } else if (datatype == MPI_DOUBLE) {
        type_size = sizeof(double);
    } else if (datatype == MPI_CHAR) {
        type_size = sizeof(char);
    } else {
        if (rank == 0) {
            printf("Unsupported MPI_Datatype.\n");
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    // Allocate memory for data buffer
    void *data = malloc(length * type_size);

    // Root process initializes the data
    if (rank == 0)
        fill_array(data, length, datatype);

    // Synchronize before starting timing
    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();

    // Dispatch operation based on input argument
    if (strcmp(operation, "broadcast") == 0) {
        perform_broadcast(data, length, datatype, rank, operation_type);
    } else if (strcmp(operation, "scatter") == 0) {
        perform_scatter(data, length, datatype, rank, size, operation_type);
    } else if (strcmp(operation, "gather") == 0) {
        perform_gather(data, length, datatype, rank, size, operation_type);
    } else if (strcmp(operation, "reduce") == 0) {
        perform_reduce(data, length, datatype, rank, size, operation_type);
    }

    // Synchronize after operation and record end time
    MPI_Barrier(MPI_COMM_WORLD);
    double end = MPI_Wtime();

    // Finalize MPI
    MPI_Finalize();

    // Root process reports execution time and frees memory
    if (rank == 0) {
        printf("Execution time (ms) = %.2f\n", (end - start) * 1000);
        free(data);
    }
    
    return 0;
}
