#ifndef CLIENT_API_H
#define CLIENT_API_H

/*
 * client_api.h - High-level client API for GUI and CLI
 * 
 * Provides abstraction over network layer and game state
 * Allows both CLI and GUI clients to use the same protocol
 */

#include <stddef.h>

#define GRID_ROWS 7
#define GRID_COLS 9

/* ==================== Connection ==================== */

/* Connect to server and start network thread
 * Returns: 0 on success, -1 on failure */
int client_connect(const char *host, int port);

/* Disconnect from server */
void client_disconnect(void);

/* Check if currently connected */
int client_is_connected(void);

/* ==================== Commands ==================== */

/* Send player name to server */
void client_send_name(const char *name);

/* Request ship placement
 * dir: 'H' for horizontal, 'V' for vertical */
void client_send_place(int r, int c, int len, char dir);

/* Request ship move (before game starts)
 * dir: destination direction ('H' or 'V') */
void client_send_move(int from_r, int from_c, int to_r, int to_c, char dir);

/* Signal ready to start game */
void client_send_ready(void);

/* Fire at opponent grid */
void client_send_fire(int r, int c);

/* Request rematch after game ends */
void client_send_play_again(int yes);

/* ==================== State Queries ==================== */

/* Get own grid
 * Returns: pointer to grid[GRID_ROWS][GRID_COLS]
 * Cell values: 0=empty, 1-5=ships, 'H'=hit, 'M'=miss */
char* client_get_own_grid(void);

/* Get opponent's revealed grid
 * Returns: pointer to grid[GRID_ROWS][GRID_COLS]
 * Only shows 'H' and 'M', other cells are empty */
char* client_get_opponent_grid(void);

/* Get current turn player ID (0 or 1) */
int client_get_current_turn(void);

/* Get player name by ID (0 or 1) */
const char* client_get_player_name(int player_id);

/* Get my player ID (0 or 1) */
int client_get_my_id(void);

/* Get current game state */
int client_get_game_state(void);
/* State values:
   0 = Connecting
   1 = Waiting for opponent
   2 = Placement phase
   3 = Combat phase
   4 = Game over
*/

/* ==================== Event Callbacks ==================== */

typedef struct {
    /* Called when player name is received from server */
    void (*on_name_received)(int player_id, const char *name);
    
    /* Called when grid(s) are updated */
    void (*on_grid_update)(void);
    
    /* Called when placement phase begins */
    void (*on_placement_start)(void);
    
    /* Called when a player places a ship */
    void (*on_player_placed)(int player_id, int length);
    
    /* Called when both players are ready, game starts */
    void (*on_game_start)(void);
    
    /* Called when turn changes */
    void (*on_turn_change)(int player_id);
    
    /* Called with result of our fire attempt
     * hit: 1 if hit, 0 if miss, -1 if already fired */
    void (*on_fire_result)(int row, int col, int hit);
    
    /* Called when opponent fires at us
     * hit: 1 if hit, 0 if miss */
    void (*on_opponent_fire)(int row, int col, int hit);
    
    /* Called when a ship is sunk
     * player_id: 0 or 1
     * length: size of destroyed ship */
    void (*on_ship_sunk)(int player_id, int length);
    
    /* Called when game ends */
    void (*on_game_end)(int winner_id);
    
    /* Called when game restarts (rematch) */
    void (*on_game_reset)(void);

    /* Called when opponent disconnects */
    void (*on_opponent_disconnected)(void);
    
    /* Called with status messages to display */
    void (*on_message)(const char *msg);
} ClientCallbacks;

/* Register callback handlers */
void client_set_callbacks(ClientCallbacks *callbacks);

#endif /* CLIENT_API_H */
