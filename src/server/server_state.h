#ifndef SERVER_STATE_H
#define SERVER_STATE_H

#include "common.h"
#include "game.h"
#include <pthread.h>

#define MAX_PLAYERS_PER_GAME 2
/* Alias MAX_CLIENTS for older code compatibility */
#define MAX_CLIENTS MAX_PLAYERS_PER_GAME

#define MAX_LOBBIES 50
#define MAX_CONNECTIONS 100

/* Forward declaration */
struct GameLobby;

/* Client context for threading */
typedef struct ClientCtx {
    sock_t fd;
    int connection_id;      // Unique ID on the server (0 to MAX_CONNECTIONS-1)
    int player_id_in_game;  // 0 or 1 within a game
    struct GameLobby *lobby; // NULL if not in a game
    char pending_name[64];   // Name stored before joining a lobby
} ClientCtx;

/* Game State (One instance of a game) */
typedef struct GameState {
    Grid *grids[MAX_PLAYERS_PER_GAME];
    int placed_count[MAX_PLAYERS_PER_GAME];
    int remaining[MAX_PLAYERS_PER_GAME][6];
    int ready[MAX_PLAYERS_PER_GAME];
    int current_turn;
    int ship_lengths[MAX_PLAYERS_PER_GAME][6];
    int rematch_response[MAX_PLAYERS_PER_GAME]; /* 0=none, 1=yes, 2=no */
} GameState;

/* Lobby (Wrapper around a game) */
typedef struct GameLobby {
    int id;
    char lobby_name[64];    // Descriptive name for the lobby
    int num_players;
    sock_t clients[MAX_PLAYERS_PER_GAME];
    char names[MAX_PLAYERS_PER_GAME][64];
    GameState *game_state;
    pthread_mutex_t lock;
} GameLobby;

/* Global Server State (Manages everything) */
typedef struct GlobalState {
    pthread_mutex_t lock;
    GameLobby *lobbies[MAX_LOBBIES];
    sock_t connections[MAX_CONNECTIONS];
    struct ClientCtx *client_contexts[MAX_CONNECTIONS]; // Look up context by connection ID
    int active_connections;
    int active_threads; // For cleanup
} GlobalState;

extern GlobalState *g_global_state;

GlobalState *global_state_create(void);
struct GameLobby *create_lobby(GlobalState *gs);
void destroy_lobby(GlobalState *gs, int lobby_id);

/* OLD API COMPATIBILITY MAPPING */
typedef struct GameLobby ServerState; 

#endif /* SERVER_STATE_H */
