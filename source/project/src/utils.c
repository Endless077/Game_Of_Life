#include <stdlib.h>
#include <time.h>

#include "utils.h"

/**
 * @brief Return high‐resolution wall‐clock time.
 *
 * This is just a thin wrapper around MPI_Wtime(). It can be used
 * to measure elapsed time of computations.
 */
double get_time(void) {
    return MPI_Wtime();
}

/**
 * @brief Initialize the RNG seed.
 *
 * If the user passes a positive seed, that value is used. Otherwise,
 * the current time (time(NULL)) is used. Internally calls srand() so that
 * any subsequent calls to rand() produce reproducible (or time‐based) sequences.
 *
 * @param user_seed User‐provided seed (if <= 0, auto‐generate).
 * @return The actual seed used.
 */
unsigned int init_seed(int user_seed) {
    unsigned int seed;
    if (user_seed > 0) {
        seed = (unsigned int)user_seed;
    } else {
        seed = (unsigned int)time(NULL);
    }
    srand(seed);
    return seed;
}
