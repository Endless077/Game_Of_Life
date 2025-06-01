//    _____      _     ___                  
//   |_   _|    (_)  .' ..]                 
//     | |      __  _| |_  .---.     .---.  
//     | |   _ [  |'-| |-'/ /__\\   / /'`\] 
//    _| |__/ | | |  | |  | \__., _ | \__.  
//   |________|[___][___]  '.__.'(_)'.___.' 
//                                          

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "life.h"

/* ********************************************************************************************* */

char* life_create(int rows, int cols, unsigned int seed) {
    int size = rows * cols;
    char *board = (char *)malloc(size * sizeof(char));
    if (!board) return NULL;

    // Use a local copy of seed for rand_r
    unsigned int state = seed > 0 ? seed : (unsigned int)time(NULL);

    // Set the current alive cells to 0
    long alive_count = 0;

    // Rejection sampling: repeat until at least one cell is alive
    do {
        alive_count = 0;
        for (int i = 0; i < size; i++) {
            board[i] = (char)(rand_r(&state) % 2);
            alive_count += board[i];
        }
    } while (alive_count == 0);

    return board;
}

void life_destroy(char *board) {
    if (board) {
        free(board);
    }
}

long life_count(const char *board, int size) {
    long count = 0;
    for (int i = 0; i < size; i++) {
        count += (board[i] == 1);
    }
    return count;
}

void life_step(const char *current, char *next, int rows, int cols) {
    // Init a board scan of the current section
    for (int i = 1; i <= rows; i++) {
        for (int j = 0; j < cols; j++) {
            int alive_neighbors = 0;

            // Scan the 3×3 neighborhood: rows i-1, i, i+1
            for (int di = -1; di <= 1; di++) {
                int row_idx = i + di;
                for (int dj = -1; dj <= 1; dj++) {
                    int col_idx = j + dj;
                    if (di == 0 && dj == 0) continue;
                    if (col_idx < 0 || col_idx >= cols) continue;
                    
                    // Linear index into flat array of size (rows+2)*cols
                    alive_neighbors += current[row_idx * cols + col_idx];
                }
            }

            // Current cell state
            char is_alive = current[i * cols + j];
            char new_state = 0;

            if (is_alive) {
                if (alive_neighbors < 2) {
                    new_state = 0;  // underpopulation
                } else if (alive_neighbors <= 3) {
                    new_state = 1;  // survival
                } else {
                    new_state = 0;  // overpopulation
                }
            } else {
                if (alive_neighbors == 3) {
                    new_state = 1;  // reproduction
                } else {
                    new_state = 0;  // remains dead
                }
            }
            next[i * cols + j] = new_state;
        }
    }
}

/* ********************************************************************************************* */
