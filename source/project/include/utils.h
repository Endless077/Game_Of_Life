//   _____  _____  _    _   __             __       
//  |_   _||_   _|/ |_ (_) [  |           [  |      
//    | |    | | `| |-'__   | |  .--.      | |--.   
//    | '    ' |  | | [  |  | | ( (`\]     | .-. |  
//     \ \__/ /   | |, | |  | |  `'.'.  _  | | | |  
//      `.__.'    \__/[___][___][\__) )(_)[___]|__] 
//                                                  

#ifndef UTILS_H
#define UTILS_H

#include <mpi.h>

/**
 * @brief Initialize the RNG seed.
 *
 * If user_seed > 0, use it directly; otherwise use the current time.
 * Calls srand() internally so that subsequent rand()/rand_r() calls are seeded.
 *
 * @param user_seed User‐provided seed (or <= 0 to auto‐generate).
 * @return The actual seed used (either user_seed or time(NULL)).
 */
unsigned int init_seed(int user_seed);

/**
 * @brief Return high‐resolution wall‐clock time.
 *
 * Wraps MPI_Wtime() to get a portable, high‐precision timer.
 *
 * @return Current time in seconds since an arbitrary MPI epoch.
 */
double get_time(void);

#endif // UTILS_H
