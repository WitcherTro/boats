#ifndef SERVER_STATE_H
#define SERVER_STATE_H

#include "common.h"
#include "game.h"
#include <pthread.h>

#define MAX_CLIENTS 2

/* Client context for threading */
typedef struct ClientCtx {
    sock_t fd;
    int id;
} ClientCtx;

/* Server game state */
typedef struct ServerState {
    pthread_mutex_t lock;
    sock_t clients[MAX_CLIENTS];
    char names[MAX_CLIENTS][64];
    Grid *grids[MAX_CLIENTS];
    int placed_count[MAX_CLIENTS];
    int remaining[MAX_CLIENTS][6];
    int ready[MAX_CLIENTS];
    int current_turn;
    /* Track each ship's length for each player (indexed by cell value 1-5) */
    int ship_lengths[MAX_CLIENTS][6];
    
    /* Rematch tracking */
    int rematch_response[MAX_CLIENTS]; /* 0=none, 1=yes, 2=no */

    /* Active thread count for clean shutdown */
    int active_threads;
} ServerState;

/* Global server state */
extern ServerState *g_server_state;

/* Initialize server state */
ServerState *server_state_create(void);

/* Destroy server state */
void server_state_destroy(ServerState *state);

#endif /* SERVER_STATE_H */
