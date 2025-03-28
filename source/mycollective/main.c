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
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (!validate_input(argc, argv, rank)) {
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    int length = atoi(argv[1]);
    char *type = argv[2];
    char *operation = argv[3];
    char *operation_type = argv[4];

    MPI_Datatype datatype = get_mpi_datatype(type);

    int type_size = (datatype == MPI_INT) ? sizeof(int) : sizeof(char);

    void *data = malloc(length * type_size);

    if (rank == 0) {
        fill_array(data, length, datatype);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();

    if (strcmp(operation, "broadcast") == 0) {
        perform_broadcast(data, length, datatype, rank, operation_type);
    } else if (strcmp(operation, "scatter") == 0) {
        perform_scatter(data, length, datatype, rank, size, operation_type);
    } else if (strcmp(operation, "gather") == 0) {
        perform_gather(data, length, datatype, rank, size, operation_type);
        } else if (strcmp(operation, "ring") == 0) {
        if (rank == 0) {
            fprintf(stderr, "Error: Ring operation not yet implemented.\n");
            MPI_Finalize();
            exit(EXIT_FAILURE);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double end = MPI_Wtime();

    free(data);
    MPI_Finalize();

    if (rank == 0) {
        printf("Execution time (ms) = %f\n", (end - start) * 1000);
    }
    
    return 0;
}
