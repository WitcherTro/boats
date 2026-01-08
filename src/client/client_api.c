/*
 * client_api.c - Implementation of high-level client API
 * 
 * Wraps network layer and provides unified interface for GUI/CLI
 * Bridges raylib GUI with existing client infrastructure
 */

#include "client_api.h"
#include "client_state.h"
#include "common.h"
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <locale.h>

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms)*1000)
#endif

/* Global callback handlers */
static ClientCallbacks g_callbacks = {0};
static pthread_t recv_tid;

/* Forward declaration for recv_thread (defined in client_recv.c) */
void *recv_thread(void *arg);

/* ==================== Connection ==================== */

int client_connect(const char *host, int port) {
    struct sockaddr_in server_addr;
    
    /* Initialize network */
    if (sock_init() != 0) {
        fprintf(stderr, "Network initialization failed\n");
        return -1;
    }
    
    /* Create socket */
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == SOCKET_INVALID) {
        fprintf(stderr, "Failed to create socket\n");
        return -1;
    }
    
    /* Setup server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((unsigned short)port);
    
    /* Convert host string to address */
    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid host address: %s\n", host);
        CLOSE(sockfd);
        return -1;
    }
    
    /* Connect to server */
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        fprintf(stderr, "Failed to connect to %s:%d\n", host, port);
        CLOSE(sockfd);
        sockfd = SOCKET_INVALID;
        return -1;
    }
    
    /* Initialize grids */
    memset(own_grid, '.', sizeof(own_grid));
    memset(opp_grid, '.', sizeof(opp_grid));
    my_id = -1;
    
    /* Start receive thread */
    if (pthread_create(&recv_tid, NULL, recv_thread, NULL) != 0) {
        fprintf(stderr, "Failed to create receive thread\n");
        CLOSE(sockfd);
        sockfd = SOCKET_INVALID;
        return -1;
    }
    
    return 0;
}

void client_disconnect(void) {
    if (sockfd != SOCKET_INVALID) {
        client_running = 0;
        shutdown(sockfd, SHUT_RDWR_FLAG);
        CLOSE(sockfd);
        sockfd = SOCKET_INVALID;
        pthread_join(recv_tid, NULL);
    }
}

int client_is_connected(void) {
    return (sockfd != SOCKET_INVALID) ? 1 : 0;
}

/* ==================== Commands ==================== */

void client_send_name(const char *name) {
    char msg[256];
    int len;
    
    if (!name || strlen(name) == 0) return;
    
    len = snprintf(msg, sizeof(msg), "NAME %s\n", name);
    if (len > 0 && sockfd != SOCKET_INVALID) {
        WRITE(sockfd, msg, len);
    }
    
    /* Store our name locally */
    strncpy(player_names[0], name, sizeof(player_names[0]) - 1);
    player_names[0][sizeof(player_names[0]) - 1] = '\0';
}

void client_send_place(int r, int c, int len, char dir) {
    char msg[128];
    int msg_len;
    
    if (sockfd == SOCKET_INVALID) return;
    
    msg_len = snprintf(msg, sizeof(msg), "PLACE %d %d %d %c\n", r, c, len, dir);
    if (msg_len > 0) {
        WRITE(sockfd, msg, msg_len);
    }
}

void client_send_move(int from_r, int from_c, int to_r, int to_c, char dir) {
    char msg[128];
    int msg_len;
    
    if (sockfd == SOCKET_INVALID) return;
    
    msg_len = snprintf(msg, sizeof(msg), "MOVE %d %d %d %d %c\n", 
                       from_r, from_c, to_r, to_c, dir);
    if (msg_len > 0) {
        WRITE(sockfd, msg, msg_len);
    }
}

void client_send_ready(void) {
    if (sockfd == SOCKET_INVALID) return;
    WRITE(sockfd, "READY\n", 6);
}

void client_send_fire(int r, int c) {
    char msg[64];
    int len;
    
    if (sockfd == SOCKET_INVALID) return;
    
    len = snprintf(msg, sizeof(msg), "FIRE %d %d\n", r, c);
    if (len > 0) {
        WRITE(sockfd, msg, len);
    }
}

void client_send_play_again(int yes) {
    if (sockfd == SOCKET_INVALID) return;
    
    if (yes) {
        WRITE(sockfd, "PLAY_AGAIN YES\n", 15);
    } else {
        WRITE(sockfd, "PLAY_AGAIN NO\n", 14);
    }
}

/* ==================== State Queries ==================== */

char* client_get_own_grid(void) {
    return (char*)own_grid;
}

char* client_get_opponent_grid(void) {
    return (char*)opp_grid;
}

int client_get_current_turn(void) {
    /* This would need to be tracked in client_state
     * For now, return -1 (unknown)
     * Could be enhanced by parsing server messages
     */
    return -1;
}

const char* client_get_player_name(int player_id) {
    if (player_id >= 0 && player_id < 2) {
        return player_names[player_id];
    }
    return "";
}

int client_get_my_id(void) {
    return my_id;
}

int client_get_game_state(void) {
    /* Return a simple state value based on current conditions
     * Could be enhanced with more tracking
     */
    if (sockfd == SOCKET_INVALID) {
        return 0;  /* Disconnected */
    }
    if (my_id < 0) {
        return 1;  /* Connected, waiting for ID assignment */
    }
    return 2;  /* In game */
}

/* ==================== Callbacks ==================== */

void client_set_callbacks(ClientCallbacks *callbacks) {
    if (callbacks) {
        memcpy(&g_callbacks, callbacks, sizeof(ClientCallbacks));
    }
}

/* Internal callback invocations (called from client_recv.c and other places) */

void api_callback_name_received(int player_id, const char *name) {
    if (g_callbacks.on_name_received) {
        g_callbacks.on_name_received(player_id, name);
    }
}

void api_callback_grid_update(void) {
    if (g_callbacks.on_grid_update) {
        g_callbacks.on_grid_update();
    }
}

void api_callback_placement_start(void) {
    if (g_callbacks.on_placement_start) {
        g_callbacks.on_placement_start();
    }
}

void api_callback_player_placed(int player_id, int length) {
    if (g_callbacks.on_player_placed) {
        g_callbacks.on_player_placed(player_id, length);
    }
}

void api_callback_game_start(void) {
    if (g_callbacks.on_game_start) {
        g_callbacks.on_game_start();
    }
}

void api_callback_turn_change(int player_id) {
    current_turn = player_id;
    if (g_callbacks.on_turn_change) {
        g_callbacks.on_turn_change(player_id);
    }
}

void api_callback_fire_result(int row, int col, int hit) {
    if (g_callbacks.on_fire_result) {
        g_callbacks.on_fire_result(row, col, hit);
    }
}

void api_callback_opponent_fire(int row, int col, int hit) {
    if (g_callbacks.on_opponent_fire) {
        g_callbacks.on_opponent_fire(row, col, hit);
    }
}

void api_callback_ship_sunk(int player_id, int length) {
    if (g_callbacks.on_ship_sunk) {
        g_callbacks.on_ship_sunk(player_id, length);
    }
}

void api_callback_game_end(int winner_id) {
    if (g_callbacks.on_game_end) {
        g_callbacks.on_game_end(winner_id);
    }
}

void api_callback_game_reset(void) {
    if (g_callbacks.on_game_reset) {
        g_callbacks.on_game_reset();
    }
}

void api_callback_opponent_disconnected(void) {
    if (g_callbacks.on_opponent_disconnected) {
        g_callbacks.on_opponent_disconnected();
    }
}

void api_callback_message(const char *msg) {
    if (g_callbacks.on_message) {
        g_callbacks.on_message(msg);
    }
}
