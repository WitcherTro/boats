#ifndef CLIENT_STATE_H
#define CLIENT_STATE_H

#include "common.h"

/*
 * client_state.h - Global client game state
 */

#define GRID_ROWS 7
#define GRID_COLS 9

/* Network socket */
extern sock_t sockfd;

/* Player identity and grids */
extern int my_id;
extern char own_grid[GRID_ROWS][GRID_COLS];
extern char opp_grid[GRID_ROWS][GRID_COLS];

/* Game state */
extern volatile int client_running;
extern volatile int server_disconnected;
extern volatile int current_turn;
extern volatile int waiting_rematch;
extern volatile int awaiting_restart;

/* Ship management */
extern const int allowed_init[6];
extern int remaining[6];
extern char player_names[2][64];

/* Track each ship's length (indexed by cell value 1-5) */
extern int ship_lengths[6];

/* Track how many ships we've placed successfully */
extern int placed_count;

/* Initialize/Reset grids and game state */
void init_grids(void);

#endif /* CLIENT_STATE_H */
