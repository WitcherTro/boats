#define _POSIX_C_SOURCE 200112L
#include "client_recv.h"
#include "client_state.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* External callback functions from client_api.c */
extern void api_callback_name_received(int player_id, const char *name);
extern void api_callback_grid_update(void);
extern void api_callback_placement_start(void);
extern void api_callback_player_placed(int player_id, int length);
extern void api_callback_game_start(void);
extern void api_callback_turn_change(int player_id);
extern void api_callback_fire_result(int row, int col, int hit);
extern void api_callback_opponent_fire(int row, int col, int hit);
extern void api_callback_ship_sunk(int player_id, int length);
extern void api_callback_game_end(int winner_id);
extern void api_callback_game_reset(void);
extern void api_callback_opponent_disconnected(void);
extern void api_callback_message(const char *msg);

#define GRID_ROWS 7
#define GRID_COLS 9

/* Track current turn for display ordering */
static int pending_turn_player = -1;
static int game_started = 0;

static void log_msg(const char *fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    api_callback_message(buf);
}

void *recv_thread(void *arg) {
    char buf[MAX_LINE];
    ssize_t n;
    while ((n = read_line(sockfd, buf, sizeof(buf))) > 0) {
        /* Parse server messages */
        int a, b, ok, who;

        if (sscanf(buf, "ASSIGN %d", &who) == 1) {
            my_id = who;
            log_msg("Assigned id %d", my_id);
            init_grids();
            continue;
        }

        int r, c, len, okv;
        char dir;
        if (sscanf(buf, "PLACED %d %d %d %c %d", &r, &c, &len, &dir, &okv) == 5) {
            if (okv) {
                /* Increment placed count and use as ship ID (1-5) */
                placed_count++;
                unsigned char ship_val = (unsigned char)placed_count;
                
                for (int i = 0; i < len; ++i) {
                    int rr = r + (dir == 'V' || dir == 'v' ? i : 0);
                    int cc = c + (dir == 'H' || dir == 'h' ? i : 0);
                    if (rr >= 0 && rr < GRID_ROWS && cc >= 0 && cc < GRID_COLS)
                        own_grid[rr][cc] = ship_val;
                }
                api_callback_grid_update();
                log_msg("Placement ok: %d,%d len %d %c", r + 1, c + 1, len, dir);
            } else {
                log_msg("Placement failed: %d,%d len %d %c", r + 1, c + 1, len, dir);
            }
            continue;
        }
        
        /* SHIP_INFO - store ship length for display */
        {
            int ship_r, ship_c, ship_len;
            if (sscanf(buf, "SHIP_INFO %d %d %d", &ship_r, &ship_c, &ship_len) == 3) {
                /* Find the ship ID at this position and store its length */
                if (ship_r >= 0 && ship_r < GRID_ROWS && ship_c >= 0 && ship_c < GRID_COLS) {
                    unsigned char ship_id = own_grid[ship_r][ship_c];
                    if (ship_id >= 1 && ship_id <= 5) {
                        ship_lengths[(int)ship_id] = ship_len;
                    }
                }
                continue;
            }
        }

        {
            int pid, r2, r3, r4, r5;
            if (sscanf(buf, "REMAIN %d 2 %d 3 %d 4 %d 5 %d", &pid, &r2, &r3, &r4, &r5) == 5) {
                if (pid == my_id) {
                    remaining[2] = r2;
                    remaining[3] = r3;
                    remaining[4] = r4;
                    remaining[5] = r5;
                }
                log_msg("Remaining ships for player %d: 2:%d 3:%d 4:%d 5:%d", pid, r2, r3, r4, r5);
                continue;
            }
        }

        /* NAME mapping from server: NAME <id> <name> */
        {
            int pid;
            char nm[64];
            if (sscanf(buf, "NAME %d %63[^\r\n]", &pid, nm) == 2) {
                strncpy(player_names[pid], nm, sizeof(player_names[pid]) - 1);
                player_names[pid][sizeof(player_names[pid]) - 1] = '\0';
                api_callback_name_received(pid, nm);
                continue;
            }
        }

        /* START_PLACEMENT <sizes...> - server asks clients to place ships now */
        if (strncmp(buf, "START_PLACEMENT", strlen("START_PLACEMENT")) == 0) {
            /* Clear grids for new game */
            init_grids();
            placed_count = 0;
            for (int l = 0; l < 6; ++l)
                ship_lengths[l] = 0;
            
            api_callback_placement_start();
            continue;
        }

        {
            int plen;
            if (sscanf(buf, "PLAYER %d PLACED %d", &who, &plen) == 2) {
                api_callback_player_placed(who, plen);
                continue;
            }
        }

        if (sscanf(buf, "ALL_PLACED %d", &who) == 1) {
            /* Don't print - server will send READY prompt */
            continue;
        }

        /* MOVE_OK - ship moved successfully */
        if (strncmp(buf, "MOVE_OK", 7) == 0) {
            int from_r, from_c, to_r, to_c;
            char dir;
            if (sscanf(buf, "MOVE_OK %d %d %d %d %c", &from_r, &from_c, &to_r, &to_c, &dir) == 5) {
                /* Find the ship at source - scan for connected cells only */
                if (from_r >= 0 && from_r < GRID_ROWS && from_c >= 0 && from_c < GRID_COLS) {
                    unsigned char ship_val = own_grid[from_r][from_c];
                    if (ship_val >= 1 && ship_val <= 5) {
                        int ship_len = 1;
                        int cells_r[17], cells_c[17];
                        cells_r[0] = from_r;
                        cells_c[0] = from_c;
                        
                        /* Scan horizontally (right and left) */
                        for (int c = from_c + 1; c < GRID_COLS && own_grid[from_r][c] == ship_val; c++) {
                            cells_r[ship_len] = from_r;
                            cells_c[ship_len] = c;
                            ship_len++;
                        }
                        for (int c = from_c - 1; c >= 0 && own_grid[from_r][c] == ship_val; c--) {
                            cells_r[ship_len] = from_r;
                            cells_c[ship_len] = c;
                            ship_len++;
                        }
                        
                        /* If still length 1, scan vertically (down and up) */
                        if (ship_len == 1) {
                            for (int r = from_r + 1; r < GRID_ROWS && own_grid[r][from_c] == ship_val; r++) {
                                cells_r[ship_len] = r;
                                cells_c[ship_len] = from_c;
                                ship_len++;
                            }
                            for (int r = from_r - 1; r >= 0 && own_grid[r][from_c] == ship_val; r--) {
                                cells_r[ship_len] = r;
                                cells_c[ship_len] = from_c;
                                ship_len++;
                            }
                        }
                        
                        /* Clear only the connected ship cells */
                        for (int i = 0; i < ship_len; i++) {
                            own_grid[cells_r[i]][cells_c[i]] = 0;
                        }
                        
                        /* Place ship at new location - trust the server! */
                        if (dir == 'H' || dir == 'h') {
                            for (int i = 0; i < ship_len && to_c + i < GRID_COLS; i++) {
                                own_grid[to_r][to_c + i] = ship_val;
                            }
                        } else {
                            for (int i = 0; i < ship_len && to_r + i < GRID_ROWS; i++) {
                                own_grid[to_r + i][to_c] = ship_val;
                            }
                        }
                    }
                }
            }
            log_msg("Ship moved successfully");
            api_callback_grid_update();
            continue;
        }

        /* MOVE_FAIL - ship move failed */
        if (strncmp(buf, "MOVE_FAIL", 9) == 0) {
            log_msg("Move failed: %s", buf + 10);
            continue;
        }

        /* PLAYER_READY - a player signaled ready */
        if (sscanf(buf, "PLAYER_READY %d", &who) == 1) {
            if (who == my_id) {
                log_msg("You are ready");
            } else if (player_names[who][0]) {
                log_msg("%s is ready", player_names[who]);
            } else {
                log_msg("Player %d is ready", who);
            }
            continue;
        }

        /* NOT_READY - tried to ready without all ships */
        if (strncmp(buf, "NOT_READY", 9) == 0) {
            log_msg("You must place all ships before signaling ready");
            continue;
        }

        /* START_FIRING - show rules and display grid */
        if (strncmp(buf, "START_FIRING", strlen("START_FIRING")) == 0) {
            game_started = 1;
            api_callback_game_start();
            continue;
        }

        if (strncmp(buf, "START", 5) == 0) {
            log_msg("All your ships are placed. Game Started.");
            continue;
        }

        if (sscanf(buf, "TURN %d", &who) == 1) {
            pending_turn_player = who;
            api_callback_turn_change(who);
            continue;
        }

        if (sscanf(buf, "RESULT %d %d %d", &a, &b, &ok) == 3) {
            /* Target receives RESULT */
            if (ok) {
                own_grid[a][b] = 'H';
            } else {
                own_grid[a][b] = 'M';
            }
            api_callback_opponent_fire(a, b, ok);
            api_callback_grid_update();
            continue;
        }

        if (sscanf(buf, "FIRE_ACK %d %d %d", &a, &b, &ok) == 3) {
            /* Attacker receives ack about opponent */
            if (ok) {
                opp_grid[a][b] = 'H';
            } else {
                opp_grid[a][b] = 'M';
            }
            api_callback_fire_result(a, b, ok);
            api_callback_grid_update();
            continue;
        }        
        /* SHIP_SUNK - a ship was destroyed */
        {
            int sunk_player, sunk_len;
            if (sscanf(buf, "SHIP_SUNK %d %d", &sunk_player, &sunk_len) == 2) {
                api_callback_ship_sunk(sunk_player, sunk_len);
                continue;
            }
        }
        if (sscanf(buf, "ALREADY_FIRED %d %d", &a, &b) == 2) {
            log_msg("You already fired at %d,%d - choose a different target", a + 1, b + 1);
            continue;
        }

        if (strncmp(buf, "HIT_YOUR_TURN", 13) == 0) {
            log_msg("It was a HIT - fire again");
            continue;
        }

        if (strncmp(buf, "HIT_OPPONENT_TURN", strlen("HIT_OPPONENT_TURN")) == 0) {
            log_msg("It was a HIT - opponent will fire again");
            continue;
        }

        if (sscanf(buf, "WIN %d", &who) == 1) {
            api_callback_game_end(who);
            waiting_rematch = 1;
            continue;
        }

        {
            int r, c, val;
            if (sscanf(buf, "REVEAL %d %d %d", &r, &c, &val) == 3) {
                opp_grid[r][c] = (char)val;
                api_callback_grid_update();
                continue;
            }
        }

        if (strncmp(buf, "PLAY_AGAIN", 10) == 0) {
            /* Server is asking whether to play again */
            if (!waiting_rematch) {
                waiting_rematch = 1;
                log_msg("Play again? Type Y or N and press Enter.");
            }
            continue;
        }

        if (strncmp(buf, "RESTART", 7) == 0) {
            /* Server restarted the game: reset local state and allow input */
            init_grids();
            waiting_rematch = 0;
            awaiting_restart = 0;
            game_started = 0;
            placed_count = 0;
            for (int l = 0; l < 6; ++l) {
                remaining[l] = allowed_init[l];
                ship_lengths[l] = 0;
            }
            api_callback_game_reset();
            continue;
        }

        if (strncmp(buf, "OPPONENT_DISCONNECTED", 21) == 0) {
            api_callback_opponent_disconnected();
            
            /* Reset local state */
            init_grids();
            waiting_rematch = 0;
            awaiting_restart = 0;
            game_started = 0;
            placed_count = 0;
            for (int l = 0; l < 6; ++l) {
                remaining[l] = allowed_init[l];
                ship_lengths[l] = 0;
            }
            continue;
        }

        if (strncmp(buf, "GAME_OVER", 9) == 0) {
            log_msg("Game over - server ended the session.");
            log_msg("Press Enter to exit.");
            server_disconnected = 1;
            CLOSE(sockfd);
            return NULL;
        }

        if (strncmp(buf, "NOT_YOUR_TURN", 13) == 0) {
            log_msg("Not your turn");
            continue;
        }

        if (strncmp(buf, "NOT_READY", 9) == 0) {
            log_msg("Both players must place ships before firing");
            continue;
        }

        /* If message is of form "PLAYER <id> <rest>", show as client message */
        int pid;
        char rest[MAX_LINE];
        if (sscanf(buf, "PLAYER %d %511[^\\n]", &pid, rest) == 2) {
            log_msg("Client %d: %s", pid, rest);
        } else {
            log_msg("Server: %s", buf);
        }
    }

    /* Server disconnected or recv error */
    if (client_running) {
        log_msg("Server disconnected");
        log_msg("Press Enter to exit.");
        server_disconnected = 1;
        /* Do not close sockfd here, let the main thread handle cleanup to avoid race conditions */
        return NULL;
    }
    return NULL;
}
