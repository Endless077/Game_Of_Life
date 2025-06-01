//   _____  _____  _    _   __                    
//  |_   _||_   _|/ |_ (_) [  |                   
//    | |    | | `| |-'__   | |  .--.      .---.  
//    | '    ' |  | | [  |  | | ( (`\]    / /'`\] 
//     \ \__/ /   | |, | |  | |  `'.'.  _ | \__.  
//      `.__.'    \__/[___][___][\__) )(_)'.___.' 
//                                                


#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

/* ********************************************************************************************* */

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

double get_time(void) {
    return MPI_Wtime();
}

/* ********************************************************************************************* */
