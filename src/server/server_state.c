#include "server_state.h"
#include <stdlib.h>
#include <string.h>

ServerState *g_server_state = NULL;

ServerState *server_state_create(void) {
    ServerState *state = calloc(1, sizeof(ServerState));
    if (!state) return NULL;
    
    pthread_mutex_init(&state->lock, NULL);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        state->clients[i] = SOCKET_INVALID;
        state->names[i][0] = '\0';
        state->grids[i] = grid_create(GRID_ROWS, GRID_COLS, sizeof(unsigned char));
        state->placed_count[i] = 0;
        state->ready[i] = 0;
        state->rematch_response[i] = 0;
        
        /* Initialize remaining ships: sizes 2,3,3,4,5 */
        const int allowed_init[6] = {0, 0, 1, 2, 1, 1};
        for (int l = 0; l < 6; l++) {
            state->remaining[i][l] = allowed_init[l];
            state->ship_lengths[i][l] = 0;
        }
    }
    
    state->current_turn = 0;
    return state;
}

void server_state_destroy(ServerState *state) {
    if (!state) return;
    
    pthread_mutex_destroy(&state->lock);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (state->grids[i]) {
            grid_destroy(state->grids[i]);
        }
    }
    
    free(state);
}
