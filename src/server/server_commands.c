#include "server_commands.h"
#include "server_message.h"
#include "common.h"
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void handle_name_command(ServerState *state, const char *msg, int sender) {
    char namebuf[64];
    if (sscanf(msg, "NAME %63[^\r\n]", namebuf) != 1) return;
    
    strncpy(state->names[sender], namebuf, sizeof(state->names[sender]) - 1);
    state->names[sender][sizeof(state->names[sender]) - 1] = '\0';
    
    /* Broadcast name to both clients */
    char nmmsg[128];
    int nl = snprintf(nmmsg, sizeof(nmmsg), "NAME %d %s\n", sender, state->names[sender]);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (state->clients[i] != SOCKET_INVALID) {
            WRITE(state->clients[i], nmmsg, nl);
        }
    }
    
    /* Send other player's name to the sender if available */
    int other = sender ^ 1;
    if (state->names[other][0] != '\0' && state->clients[sender] != SOCKET_INVALID) {
        char other_nm_msg[128];
        int onl = snprintf(other_nm_msg, sizeof(other_nm_msg), "NAME %d %s\n", other, state->names[other]);
        WRITE(state->clients[sender], other_nm_msg, onl);
    }
    
    /* If both players have names, start placement phase */
    if (state->names[0][0] != '\0' && state->names[1][0] != '\0') {
        /* Reset ship tracking and clear grids for new game */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            /* Destroy and recreate grids to clear old data */
            if (state->grids[i]) {
                grid_destroy(state->grids[i]);
            }
            state->grids[i] = grid_create(GRID_ROWS, GRID_COLS, sizeof(unsigned char));
            
            state->placed_count[i] = 0;
            state->ready[i] = 0;
            
            /* Reset remaining ships */
            const int allowed_init[6] = {0, 0, 1, 2, 1, 1};
            for (int j = 0; j < 6; j++) {
                state->remaining[i][j] = allowed_init[j];
                state->ship_lengths[i][j] = 0;
            }
        }
        state->current_turn = 0;
        
        const char *pmsg = "START_PLACEMENT 2 3 3 4 5\n";
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (state->clients[i] != SOCKET_INVALID) {
                WRITE(state->clients[i], pmsg, (int)strlen(pmsg));
            }
        }
    }
}

void handle_place_command(ServerState *state, const char *msg, int sender) {
    int r, c, len;
    char dir = 'H';
    char um[MAX_LINE];
    
    /* Convert to uppercase for parsing */
    size_t mi = 0;
    for (size_t i = 0; i < strlen(msg) && i + 1 < sizeof(um); i++) {
        char ch = msg[i];
        if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
        um[mi++] = ch;
    }
    um[mi] = '\0';
    
    if (sscanf(um, "PLACE %d %d %d %c", &r, &c, &len, &dir) < 3) return;
    
    int ok = 0;
    if (len >= 2 && len <= 5 && state->remaining[sender][len] > 0) {
        /* Use placed_count+1 as ship ID (1-5) */
        int ship_id = state->placed_count[sender] + 1;
        Ship s = {r, c, len, dir, ship_id};
        ok = place_ship(state->grids[sender], s);
        if (ok) {
            state->remaining[sender][len]--;
            state->placed_count[sender]++;
            
            /* Store ship length for tracking */
            state->ship_lengths[sender][ship_id] = len;
            
            /* Send ship info to client so they can display ship lengths */
            char shipinfo[64];
            snprintf(shipinfo, sizeof(shipinfo), "SHIP_INFO %d %d %d\n", r, c, len);
            WRITE(state->clients[sender], shipinfo, strlen(shipinfo));
        }
    }
    
    /* Send result to sender */
    char resp[128];
    snprintf(resp, sizeof(resp), "PLACED %d %d %d %c %d\n", r, c, len, dir, ok);
    WRITE(state->clients[sender], resp, strlen(resp));
    
    /* Notify both clients on successful placement */
    if (ok) {
        snprintf(resp, sizeof(resp), "PLAYER %d PLACED %d\n", sender, len);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (state->clients[i] != SOCKET_INVALID) {
                WRITE(state->clients[i], resp, strlen(resp));
            }
        }
    }
    
    /* Send remaining counts to placing client */
    char remmsg[128];
    int rl = snprintf(remmsg, sizeof(remmsg), "REMAIN %d 2 %d 3 %d 4 %d 5 %d\n",
                      sender, state->remaining[sender][2], state->remaining[sender][3],
                      state->remaining[sender][4], state->remaining[sender][5]);
    if (state->clients[sender] != SOCKET_INVALID) {
        WRITE(state->clients[sender], remmsg, rl);
    }
    
    /* Notify both clients when a player finishes placement */
    if (ok && state->placed_count[sender] == 5) {
        char allmsg[64];
        snprintf(allmsg, sizeof(allmsg), "ALL_PLACED %d\n", sender);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (state->clients[i] != SOCKET_INVALID) {
                WRITE(state->clients[i], allmsg, strlen(allmsg));
            }
        }
        
        /* Prompt to use READY command */
        const char *readymsg = "All ships placed. Type READY when you're ready to start.\n";
        WRITE(state->clients[sender], readymsg, strlen(readymsg));
    }
}

void handle_move_command(ServerState *state, const char *msg, int sender) {
    /* Only allow moves before the game starts (before both players are ready) */
    if (state->ready[0] && state->ready[1]) {
        WRITE(state->clients[sender], "MOVE_FAIL Cannot move ships during an active game\n", 50);
        return;
    }
    
    int from_r, from_c, to_r, to_c;
    char dir = 'H';
    char um[MAX_LINE];
    
    /* Convert to uppercase for parsing */
    size_t mi = 0;
    for (size_t i = 0; i < strlen(msg) && i + 1 < sizeof(um); i++) {
        char ch = msg[i];
        if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
        um[mi++] = ch;
    }
    um[mi] = '\0';
    
    if (sscanf(um, "MOVE %d %d %d %d %c", &from_r, &from_c, &to_r, &to_c, &dir) < 4) {
        WRITE(state->clients[sender], "INVALID MOVE format\n", 20);
        return;
    }
    
    Grid *g = state->grids[sender];
    
    /* Find ship at from location */
    unsigned char from_cell = 0;
    grid_get(g, from_r, from_c, &from_cell);
    if (from_cell == 0) {
        WRITE(state->clients[sender], "MOVE_FAIL No ship at source location\n", 38);
        return;
    }
    
    /* Determine ship length by scanning both directions */
    int ship_len = 1;
    unsigned char ship_val = from_cell;
    char original_dir = 'H';  /* Track original orientation */
    
    /* Scan horizontally (left and right) */
    for (int c = from_c + 1; c < GRID_COLS; c++) {
        unsigned char cell = 0;
        grid_get(g, from_r, c, &cell);
        if (cell == ship_val) ship_len++;
        else break;
    }
    for (int c = from_c - 1; c >= 0; c--) {
        unsigned char cell = 0;
        grid_get(g, from_r, c, &cell);
        if (cell == ship_val) ship_len++;
        else break;
    }
    
    /* If still length 1, scan vertically (up and down) */
    if (ship_len == 1) {
        original_dir = 'V';
        for (int r = from_r + 1; r < GRID_ROWS; r++) {
            unsigned char cell = 0;
            grid_get(g, r, from_c, &cell);
            if (cell == ship_val) ship_len++;
            else break;
        }
        for (int r = from_r - 1; r >= 0; r--) {
            unsigned char cell = 0;
            grid_get(g, r, from_c, &cell);
            if (cell == ship_val) ship_len++;
            else break;
        }
    }
    
    /* Find the actual start position of the ship */
    int orig_r = from_r, orig_c = from_c;
    if (original_dir == 'H') {
        /* Scan left to find start */
        while (orig_c > 0) {
            unsigned char cell = 0;
            grid_get(g, orig_r, orig_c - 1, &cell);
            if (cell == ship_val) orig_c--;
            else break;
        }
    } else {
        /* Scan up to find start */
        while (orig_r > 0) {
            unsigned char cell = 0;
            grid_get(g, orig_r - 1, orig_c, &cell);
            if (cell == ship_val) orig_r--;
            else break;
        }
    }
    
    /* Remove old ship */
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            unsigned char cell = 0;
            grid_get(g, r, c, &cell);
            if (cell == ship_val) {
                unsigned char empty = 0;
                grid_set(g, r, c, &empty);
            }
        }
    }
    
    /* Restore remaining count */
    if (ship_len >= 2 && ship_len <= 5) {
        state->remaining[sender][ship_len]++;
        state->placed_count[sender]--;
    }
    
    /* Try to place at new location with same ship ID */
    Ship s = {to_r, to_c, ship_len, dir, ship_val};
    int ok = place_ship(g, s);
    
    if (ok) {
        state->remaining[sender][ship_len]--;
        state->placed_count[sender]++;
        
        char resp[128];
        snprintf(resp, sizeof(resp), "MOVE_OK %d %d %d %d %c\n", from_r, from_c, to_r, to_c, dir);
        WRITE(state->clients[sender], resp, strlen(resp));
    } else {
        /* Restore old ship if move failed with ORIGINAL position and direction */
        Ship old_s = {orig_r, orig_c, ship_len, original_dir, ship_val};
        int restored = place_ship(g, old_s);
        state->remaining[sender][ship_len]--;
        state->placed_count[sender]++;
        
        /* Debug: send detailed failure info */
        char resp[128];
        snprintf(resp, sizeof(resp), "MOVE_FAIL Cannot place ship at new location (restored=%d)\n", restored);
        WRITE(state->clients[sender], resp, strlen(resp));
    }
}

void handle_ready_command(ServerState *state, int sender) {
    /* Check if player has placed all ships */
    if (state->placed_count[sender] != 5) {
        WRITE(state->clients[sender], "NOT_READY You must place all ships first\n", 42);
        return;
    }
    
    state->ready[sender] = 1;
    
    /* Notify both players */
    char readymsg[64];
    snprintf(readymsg, sizeof(readymsg), "PLAYER_READY %d\n", sender);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (state->clients[i] != SOCKET_INVALID) {
            WRITE(state->clients[i], readymsg, strlen(readymsg));
        }
    }
    
    /* Check if both players are ready */
    if (state->ready[0] && state->ready[1]) {
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (state->clients[i] != SOCKET_INVALID) {
                WRITE(state->clients[i], "START\n", 6);
            }
        }
        
        char tmsg[32];
        snprintf(tmsg, sizeof(tmsg), "TURN %d\n", state->current_turn);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (state->clients[i] != SOCKET_INVALID) {
                WRITE(state->clients[i], tmsg, strlen(tmsg));
            }
        }
        
        /* Send firing instructions */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (state->clients[i] != SOCKET_INVALID) {
                WRITE(state->clients[i], "START_FIRING\n", (int)strlen("START_FIRING\n"));
            }
        }
    }
}

void handle_fire_command(ServerState *state, const char *msg, int sender) {
    int r, c;
    char um[MAX_LINE];
    
    /* Convert to uppercase for parsing */
    size_t mi = 0;
    for (size_t i = 0; i < strlen(msg) && i + 1 < sizeof(um); i++) {
        char ch = msg[i];
        if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
        um[mi++] = ch;
    }
    um[mi] = '\0';
    
    if (sscanf(um, "FIRE %d %d", &r, &c) != 2) return;
    
    /* Validate game state */
    if (!(state->placed_count[0] == 5 && state->placed_count[1] == 5)) {
        WRITE(state->clients[sender], "NOT_READY\n", 10);
        return;
    }
    
    if (sender != state->current_turn) {
        WRITE(state->clients[sender], "NOT_YOUR_TURN\n", 15);
        return;
    }
    
    /* Execute attack */
    int target = sender ^ 1;
    
    /* Save ship ID BEFORE firing (since fire_at converts to CELL_HIT) */
    unsigned char ship_id_at_target = 0;
    grid_get(state->grids[target], r, c, &ship_id_at_target);
    
    int hit = fire_at(state->grids[target], r, c);
    
    if (hit == -1) {
        /* Already fired at this cell */
        char already[64];
        snprintf(already, sizeof(already), "ALREADY_FIRED %d %d\n", r, c);
        WRITE(state->clients[sender], already, (int)strlen(already));
        return;
    }
    
    /* Valid hit/miss; send results */
    char resp[128];
    snprintf(resp, sizeof(resp), "RESULT %d %d %d\n", r, c, hit);
    if (state->clients[target] != SOCKET_INVALID) {
        WRITE(state->clients[target], resp, strlen(resp));
    }
    
    snprintf(resp, sizeof(resp), "FIRE_ACK %d %d %d\n", r, c, hit);
    WRITE(state->clients[sender], resp, strlen(resp));
    
    /* If hit, check if ship is destroyed */
    if (hit && ship_id_at_target >= 1 && ship_id_at_target <= 5) {
        /* Scan entire grid to see if any cells with this ship ID remain */
        int ship_alive = 0;
        for (int scan_r = 0; scan_r < GRID_ROWS && !ship_alive; scan_r++) {
            for (int scan_c = 0; scan_c < GRID_COLS && !ship_alive; scan_c++) {
                unsigned char cell = 0;
                grid_get(state->grids[target], scan_r, scan_c, &cell);
                /* Check if this cell has the same ship ID */
                if (cell == ship_id_at_target) {
                    ship_alive = 1;
                }
            }
        }
            
        if (!ship_alive) {
            /* Ship is fully destroyed - get length from stored data */
            int sunk_len = state->ship_lengths[target][ship_id_at_target];
            
            /* Notify both players that a ship was destroyed */
            char sunkmsg[64];
            snprintf(sunkmsg, sizeof(sunkmsg), "SHIP_SUNK %d %d\n", target, sunk_len);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (state->clients[i] != SOCKET_INVALID) {
                    WRITE(state->clients[i], sunkmsg, strlen(sunkmsg));
                }
            }
        }
    }
    
    /* Check for win condition */
    if (!grid_has_ships(state->grids[target])) {
        char winmsg[64];
        snprintf(winmsg, sizeof(winmsg), "WIN %d\n", sender);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (state->clients[i] != SOCKET_INVALID) {
                WRITE(state->clients[i], winmsg, strlen(winmsg));
            }
        }

        /* Reveal grids to opponents */
        for (int player = 0; player < MAX_CLIENTS; player++) {
            int opponent = player ^ 1;
            if (state->clients[opponent] == SOCKET_INVALID) continue;

            for (int r = 0; r < GRID_ROWS; r++) {
                for (int c = 0; c < GRID_COLS; c++) {
                    unsigned char cell;
                    grid_get(state->grids[player], r, c, &cell);
                    /* If it's a ship (1-5), send REVEAL to opponent */
                    if (cell >= 1 && cell <= 5) {
                        char revmsg[64];
                        snprintf(revmsg, sizeof(revmsg), "REVEAL %d %d %d\n", r, c, cell);
                        WRITE(state->clients[opponent], revmsg, strlen(revmsg));
                    }
                }
            }
        }
        
        /* Ask both players if they want to play again */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (state->clients[i] != SOCKET_INVALID) {
                WRITE(state->clients[i], "PLAY_AGAIN\n", 11);
            }
        }
        return;
    }
    
    /* Handle turn switching or continuation */
    if (!hit) {
        /* Miss: switch turns */
        state->current_turn ^= 1;
        char tmsg[32];
        snprintf(tmsg, sizeof(tmsg), "TURN %d\n", state->current_turn);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (state->clients[i] != SOCKET_INVALID) {
                WRITE(state->clients[i], tmsg, strlen(tmsg));
            }
        }
    } else {
        /* Hit: attacker goes again */
        if (state->clients[sender] != SOCKET_INVALID) {
            WRITE(state->clients[sender], "HIT_YOUR_TURN\n", (int)strlen("HIT_YOUR_TURN\n"));
        }
        if (state->clients[target] != SOCKET_INVALID) {
            WRITE(state->clients[target], "HIT_OPPONENT_TURN\n", (int)strlen("HIT_OPPONENT_TURN\n"));
        }
    }
}

void handle_disconnect(ServerState *state, int sender, sock_t *listen_fd_ptr) {
    pthread_mutex_lock(&state->lock);
    
    /* Close the disconnected client */
    if (state->clients[sender] != SOCKET_INVALID) {
        CLOSE(state->clients[sender]);
        state->clients[sender] = SOCKET_INVALID;
    }
    
    /* Notify the other client if connected */
    int other = sender ^ 1;
    if (state->clients[other] != SOCKET_INVALID) {
        const char *msg = "OPPONENT_DISCONNECTED\n";
        WRITE(state->clients[other], msg, (int)strlen(msg));
        
        /* Reset game state for the remaining player */
        if (state->grids[other]) {
            grid_destroy(state->grids[other]);
            state->grids[other] = grid_create(GRID_ROWS, GRID_COLS, sizeof(unsigned char));
        }
        state->placed_count[other] = 0;
        state->ready[other] = 0;
        state->rematch_response[other] = 0;
        
        const int allowed_init[6] = {0, 0, 1, 2, 1, 1};
        for (int l = 0; l < 6; l++) {
            state->remaining[other][l] = allowed_init[l];
            state->ship_lengths[other][l] = 0;
        }
    }
    
    /* Reset the disconnected player's state */
    if (state->grids[sender]) {
        grid_destroy(state->grids[sender]);
        state->grids[sender] = grid_create(GRID_ROWS, GRID_COLS, sizeof(unsigned char));
    }
    state->placed_count[sender] = 0;
    state->ready[sender] = 0;
    state->rematch_response[sender] = 0;
    state->names[sender][0] = '\0';
    
    pthread_mutex_unlock(&state->lock);
}

/* Non-blocking rematch handler */
void handle_rematch_response(ServerState *state, int sender, int response) {
    pthread_mutex_lock(&state->lock);
    
    state->rematch_response[sender] = response; /* 1=YES, 2=NO */
    
    int other = sender ^ 1;
    int other_resp = state->rematch_response[other];
    
    if (response == 2) {
        /* Player said NO - disconnect them */
        WRITE(state->clients[sender], "GAME_OVER\n", 10);
        shutdown(state->clients[sender], SHUT_RDWR_FLAG);
        CLOSE(state->clients[sender]);
        state->clients[sender] = SOCKET_INVALID;
        
        /* Reset their state */
        state->names[sender][0] = '\0';
        state->rematch_response[sender] = 0;
        
        /* If other player is waiting (said YES), notify them? 
           Actually, if one says NO, the game session ends. 
           The other player should be told to wait for a new opponent. */
        if (state->clients[other] != SOCKET_INVALID) {
            const char *msg = "OPPONENT_DISCONNECTED\n";
            WRITE(state->clients[other], msg, (int)strlen(msg));
            
            /* Reset other player's game state */
            state->placed_count[other] = 0;
            state->ready[other] = 0;
            state->rematch_response[other] = 0;
            /* (Grids etc should be reset too) */
        }
    } else if (response == 1 && other_resp == 1) {
        /* Both said YES - Restart */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (state->grids[i]) {
                grid_destroy(state->grids[i]);
                state->grids[i] = grid_create(GRID_ROWS, GRID_COLS, sizeof(unsigned char));
            }
            state->placed_count[i] = 0;
            state->ready[i] = 0;
            state->rematch_response[i] = 0;
            
            const int allowed_init[6] = {0, 0, 1, 2, 1, 1};
            for (int l = 0; l < 6; l++) {
                state->remaining[i][l] = allowed_init[l];
                state->ship_lengths[i][l] = 0;
            }
            
            if (state->clients[i] != SOCKET_INVALID) {
                WRITE(state->clients[i], "RESTART\n", 8);
            }
        }
        state->current_turn = 0;
    }
    
    pthread_mutex_unlock(&state->lock);
}
