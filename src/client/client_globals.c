#include "client_state.h"
#include "common.h"
#include <string.h>

/* Global client state */
sock_t sockfd = SOCKET_INVALID;
int my_id = -1;
char own_grid[GRID_ROWS][GRID_COLS];
char opp_grid[GRID_ROWS][GRID_COLS];
volatile int client_running = 1;
volatile int server_disconnected = 0;
volatile int current_turn = -1;
volatile int waiting_rematch = 0;
volatile int awaiting_restart = 0;
/* Allowed ships: sizes 2,3,3,4,5 */
const int allowed_init[6] = {0,0,1,2,1,1};
int remaining[6];
char player_names[2][64];
/* Track each ship's length (indexed by cell value) */
int ship_lengths[6] = {0};
/* Track how many ships we've placed successfully */
int placed_count = 0;

void init_grids(void) {
    for (int r = 0; r < GRID_ROWS; ++r)
        for (int c = 0; c < GRID_COLS; ++c) {
            own_grid[r][c] = '.';
            opp_grid[r][c] = '.';
        }
    for (int l = 0; l < 6; ++l)
        remaining[l] = allowed_init[l];
}
