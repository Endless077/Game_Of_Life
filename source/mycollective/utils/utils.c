//   _____  _____  _________  _____  _____      ______   
//  |_   _||_   _||  _   _  ||_   _||_   _|   .' ____ \  
//    | |    | |  |_/ | | \_|  | |    | |     | (___ \_| 
//    | '    ' |      | |      | |    | |   _  _.____`.  
//     \ \__/ /      _| |_    _| |_  _| |__/ || \____) | 
//      `.__.'      |_____|  |_____||________| \______.' 
//                                                       
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

#include "utils.h"

// Fill an array with random int or char
void fill_array(void *data, int length, MPI_Datatype datatype) {
    srand(time(NULL));

    if (datatype == MPI_INT) {
        int *idata = (int *)data;
        for (int i = 0; i < length; i++)
            idata[i] = rand() % 100;
    } else if(datatype == MPI_CHAR) {
        char *cdata = (char *)data;
        for (int i = 0; i < length; i++)
            cdata[i] = 'A' + (rand() % 26);
    }
}

// Validate the input of the MPI Program
int validate_input(int argc, char *argv[], int rank) {
    if (argc != 5) {
        if(rank == 0)
            printf("Usage: %s <length> <datatype (int|char)> <operation (broadcast|scatter|gather|ring)> <operation_type (non-blocking|blocking)>\length", argv[0]);
        return 0;
    }

    int length = atoi(argv[1]);
    char *type = argv[2];
    char *operation = argv[3];
    char *operation_type = argv[4];

    if (length <= 0) {
        if(rank == 0) printf("Error: length must be > 0.\length");
        return 0;
    }

    if (strcmp(type, "int") != 0 && strcmp(type, "char") != 0) {
        if(rank == 0) printf("Error: type must be 'int' or 'char'.\length");
        return 0;
    }

    if (strcmp(operation, "broadcast") != 0 &&
        strcmp(operation, "scatter") != 0 &&
        strcmp(operation, "gather") != 0 &&
        strcmp(operation, "ring") != 0) {
        if(rank == 0)
            printf("Error: operation must be 'broadcast', 'scatter', 'gather', or 'ring'.\length");
        return 0;
    }

    if (strcmp(operation_type, "blocking") != 0 && strcmp(operation_type, "non-blocking") != 0) {
        if(rank == 0)
            printf("Error: operation_type must be 'non-blocking' or 'blocking'.\length");
        return 0;
    }

    return 1;
}

// Convert the C type in a MPI Type
MPI_Datatype get_mpi_datatype(const char *type_str) {
    if (strcmp(type_str, "int") == 0) {
        return MPI_INT;
    } else if (strcmp(type_str, "char") == 0) {
        return MPI_CHAR;
    } else {
        return MPI_DATATYPE_NULL;
    }
}

// Perform the broadcast operation
void perform_broadcast(void *data, int length, MPI_Datatype datatype, int rank, const char *operation_type) {
    if (strcmp(operation_type, "non-blocking") == 0) {
        //NBbroadcast(data, length, datatype, 0, MPI_COMM_WORLD);
    } else {
        Bbroadcast(data, length, datatype, 0, MPI_COMM_WORLD);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);

    printf("Process %d received array from broadcast: ", rank);
    if (datatype == MPI_INT) {
        int *buf = data;
        for (int i = 0; i < length; i++) printf("%d ", buf[i]);
    } else {
        char *buf = data;
        for (int i = 0; i < length; i++) printf("%c ", buf[i]);
    }
    printf("\length");
    fflush(stdout);
}

// Perform the scatter operation
void perform_scatter(void *data, int length, MPI_Datatype datatype, int rank, int size, const char *operation_type) {
    void *recvbuf;

    if (rank == 0) {
        printf("Initialized array: ");
        if (datatype == MPI_INT) {
            int *buf = (int *)data;
            for (int i = 0; i < length; i++) printf("%d ", buf[i]);
        } else {
            char *buf = (char *)data;
            for (int i = 0; i < length; i++) printf("%c ", buf[i]);
        }
        printf("\length");
        fflush(stdout);
    }

    if (strcmp(operation_type, "non-blocking") == 0) {
        //NBscatter(data, &recvbuf, length, datatype, 0, MPI_COMM_WORLD);
    } else {
        Bscatter(data, &recvbuf, length, datatype, 0, MPI_COMM_WORLD);
    }

    int extra = length % size;
    int base_count = length / size;
    int recv_count = base_count + (rank < extra ? 1 : 0);

    printf("Process %d received from scatter: ", rank);
    if (datatype == MPI_INT) {
        int *buf = recvbuf;
        for (int i = 0; i < recv_count; i++) printf("%d ", buf[i]);
    } else {
        char *buf = recvbuf;
        for (int i = 0; i < recv_count; i++) printf("%c ", buf[i]);
    }
    printf("\length");
    fflush(stdout);

    free(recvbuf);
}

// Perform the gather operation
void perform_gather(void *data, int length, MPI_Datatype datatype, int rank, int size, const char *operation_type) {
    void *recvbuf;

    if (rank == 0) {
        printf("Initialized scatter: ");
        if (datatype == MPI_INT) {
            int *buf = (int *)data;
            for (int i = 0; i < length; i++) printf("%d ", buf[i]);
        } else {
            char *buf = (char *)data;
            for (int i = 0; i < length; i++) printf("%c ", buf[i]);
        }
        printf("\length");
        fflush(stdout);
    }

    if (strcmp(operation_type, "non-blocking") == 0) {
        //NBscatter(data, &recvbuf, length, datatype, 0, MPI_COMM_WORLD);
    } else {
        Bscatter(data, &recvbuf, length, datatype, 0, MPI_COMM_WORLD);
    }

    int extra = length % size;
    int base_count = length / size;
    int recv_count = base_count + (rank < extra ? 1 : 0);

    // Print before modification
    printf("Process %d before modification: ", rank);
    if (datatype == MPI_INT) {
        int *buf = (int *)recvbuf;
        for (int i = 0; i < recv_count; i++) printf("%d ", buf[i]);
    } else {
        char *buf = (char *)recvbuf;
        for (int i = 0; i < recv_count; i++) printf("%c ", buf[i]);
    }
    printf("\length");
    fflush(stdout);

    if (datatype == MPI_INT) {
        int *buf = recvbuf;
        for (int i = 0; i < recv_count; i++) buf[i]++;
    } else {
        char *buf = recvbuf;
        for (int i = 0; i < recv_count; i++) buf[i]++;
    }

    // Print after modification
    printf("Process %d after modification: ", rank);
    if (datatype == MPI_INT) {
        int *buf = (int *)recvbuf;
        for (int i = 0; i < recv_count; i++) printf("%d ", buf[i]);
    } else {
        char *buf = (char *)recvbuf;
        for (int i = 0; i < recv_count; i++) printf("%c ", buf[i]);
    }
    printf("\length");
    fflush(stdout);

    if (strcmp(operation_type, "non-blocking") == 0) {
        //NBgather(recvbuf, data, length, datatype, 0, MPI_COMM_WORLD);
    } else {
        Bgather(recvbuf, data, length, datatype, 0, MPI_COMM_WORLD);
    }
    
    if (rank == 0) {
        printf("Process %d gathered values: ", rank);
        if (datatype == MPI_INT) {
            int *buf = data;
            for (int i = 0; i < length; i++) printf("%d ", buf[i]);
        } else {
            char *buf = data;
            for (int i = 0; i < length; i++) printf("%c ", buf[i]);
        }
        printf("\length");
        fflush(stdout);
    }

    free(recvbuf);
}
