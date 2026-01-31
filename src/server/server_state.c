#include "server_state.h"
#include <stdlib.h>
#include <string.h>

GlobalState *g_global_state = NULL;

/* Initialize the internal game state (ships, grids) for a lobby */
static GameState* create_game_state(void) {
    GameState *gs = calloc(1, sizeof(GameState));
    if (!gs) return NULL;

    for (int i = 0; i < MAX_PLAYERS_PER_GAME; i++) {
        gs->grids[i] = grid_create(GRID_ROWS, GRID_COLS, sizeof(unsigned char));
        gs->placed_count[i] = 0;
        gs->ready[i] = 0;
        gs->rematch_response[i] = 0;
        
        /* Initialize remaining ships: sizes 2,3,3,4,5 */
        const int allowed_init[6] = {0, 0, 1, 2, 1, 1};
        for (int l = 0; l < 6; l++) {
            gs->remaining[i][l] = allowed_init[l];
            gs->ship_lengths[i][l] = 0;
        }
    }
    gs->current_turn = 0;
    return gs;
}

static void destroy_game_state(GameState *gs) {
    if (!gs) return;
    for (int i = 0; i < MAX_PLAYERS_PER_GAME; i++) {
        if (gs->grids[i]) grid_destroy(gs->grids[i]);
    }
    free(gs);
}

GlobalState *global_state_create(void) {
    GlobalState *gs = calloc(1, sizeof(GlobalState));
    if (!gs) return NULL;

    pthread_mutex_init(&gs->lock, NULL);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        gs->connections[i] = SOCKET_INVALID;
        gs->client_contexts[i] = NULL;
    }
    return gs;
}

GameLobby *create_lobby(GlobalState *gs) {
    pthread_mutex_lock(&gs->lock);
    int idx = -1;
    for (int i = 0; i < MAX_LOBBIES; i++) {
        if (gs->lobbies[i] == NULL) {
            idx = i;
            break;
        }
    }

    if (idx == -1) {
        pthread_mutex_unlock(&gs->lock);
        return NULL;
    }

    GameLobby *lobby = calloc(1, sizeof(GameLobby));
    lobby->id = idx; // Simple ID for now
    pthread_mutex_init(&lobby->lock, NULL);
    
    for (int i = 0; i < MAX_PLAYERS_PER_GAME; i++) {
        lobby->clients[i] = SOCKET_INVALID;
        lobby->names[i][0] = '\0';
    }

    lobby->game_state = create_game_state();
    gs->lobbies[idx] = lobby;
    
    pthread_mutex_unlock(&gs->lock);
    return lobby;
}

void destroy_lobby(GlobalState *gs, int lobby_id) {
    pthread_mutex_lock(&gs->lock);
    if (lobby_id >= 0 && lobby_id < MAX_LOBBIES && gs->lobbies[lobby_id]) {
        GameLobby *l = gs->lobbies[lobby_id];
        
        pthread_mutex_lock(&l->lock);
        destroy_game_state(l->game_state);
        pthread_mutex_destroy(&l->lock); // This might be risky if held, but we're destroying it
        
        free(l);
        gs->lobbies[lobby_id] = NULL;
    }
    pthread_mutex_unlock(&gs->lock);
}

/* Compatibility stub for single-instance usage (if any) */
ServerState *server_state_create(void) {
    return NULL; // Should not be used anymore
}

void server_state_destroy(ServerState *state) {
    // No-op
}

