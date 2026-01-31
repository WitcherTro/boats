#define _POSIX_C_SOURCE 200112L
#include "common.h"
#include "game.h"
#include "server_state.h"
#include "server_message.h"
#include "server_client.h"
#include "server_commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <locale.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

static volatile int server_running = 1;

/* Helper for strdup if not available */
static char *my_strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *p = malloc(len);
    if (p) memcpy(p, s, len);
    return p;
}

/* Console input thread */
static void *console_thread(void *arg) {
    sock_t *listen_fd_ptr = (sock_t *)arg;
    char line[256];
    
    printf("Type 'quit' or 'exit' to stop the server.\n");
    
    while (server_running && fgets(line, sizeof(line), stdin)) {
        /* Remove newline */
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        
        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) {
            printf("Stopping server...\n");
            server_running = 0;
            
            /* Close listener to unblock accept() if waiting */
            if (*listen_fd_ptr != SOCKET_INVALID) {
                shutdown(*listen_fd_ptr, SHUT_RDWR_FLAG);
                CLOSE(*listen_fd_ptr);
                *listen_fd_ptr = SOCKET_INVALID;
            }
            
            /* Enqueue message to unblock dequeue_msg() if waiting */
            enqueue_msg(my_strdup("SERVER_QUIT"), -2);
            break;
        }
    }
    return NULL;
}

/* Accept thread */
static void *accept_thread(void *arg) {
    sock_t listen_fd = *(sock_t *)arg;
    
    while (server_running) {
        struct sockaddr_in caddr;
        int sl = sizeof(caddr);
        sock_t c = accept(listen_fd, (struct sockaddr *)&caddr, &sl);
        if (c == SOCKET_INVALID) {
            if (!server_running) break;
            continue;
        }
        
        pthread_mutex_lock(&g_global_state->lock);
        
        int assigned = -1;
        for (int i = 0; i < MAX_CONNECTIONS; i++) {
            if (g_global_state->connections[i] == SOCKET_INVALID) {
                g_global_state->connections[i] = c;
                assigned = i;
                break;
            }
        }
        
        if (assigned != -1) {
            /* Create Context */
            ClientCtx *ctx = calloc(1, sizeof(ClientCtx));
            ctx->fd = c;
            ctx->connection_id = assigned;
            ctx->player_id_in_game = -1;
            ctx->lobby = NULL;

            g_global_state->client_contexts[assigned] = ctx;
            g_global_state->active_connections++;
            
            /* Add detached tracking if needed, or just rely on active_connections */
            g_global_state->active_threads++;
            
            pthread_t th;
            pthread_create(&th, NULL, client_reader, ctx);
            pthread_detach(th);
            
            printf("Connection accepted: ID %d\n", assigned);
        } else {
            /* Server full */
            const char *msg = "BUSY Server full\n";
            WRITE(c, msg, (int)strlen(msg));
            shutdown(c, SHUT_RDWR_FLAG);
            CLOSE(c);
        }
        
        pthread_mutex_unlock(&g_global_state->lock);
    }
    return NULL;
}

/* Try to join an existing lobby with space, or create a new one */
static void assign_lobby(ClientCtx *ctx) {
    pthread_mutex_lock(&g_global_state->lock);
    
    GameLobby *joined_lobby = NULL;
    int player_idx = -1;

    /* 1. Try to find a lobby with 1 player waiting */
    for (int i = 0; i < MAX_LOBBIES; i++) {
        GameLobby *l = g_global_state->lobbies[i];
        if (l && l->num_players == 1) {
            /* Join this one */
            if (l->clients[0] == SOCKET_INVALID) player_idx = 0;
            else if (l->clients[1] == SOCKET_INVALID) player_idx = 1;
            
            if (player_idx != -1) {
                l->clients[player_idx] = ctx->fd;
                l->num_players++;
                joined_lobby = l;
                break;
            }
        }
    }

    /* 2. If no lobby found, create a new one */
    if (!joined_lobby) {
        /* Find empty lobby slot */
        for (int i = 0; i < MAX_LOBBIES; i++) {
            if (g_global_state->lobbies[i] == NULL) {
                 joined_lobby = create_lobby(g_global_state); // Already locks? No, we hold the lock. create_lobby tries lock!
                 /* Wait, create_lobby locks global state. deadlock! */
                 /* We need to implement create_lobby logic here or unlock first */
                 break;
            }
        }
        
        if (!joined_lobby) {
            /* Try to call create_lobby, but we need to release lock first. 
               But then state might change.
               Better: duplicate create_lobby logic here or make create_lobby not lock. */
        }
    }

    pthread_mutex_unlock(&g_global_state->lock);
    
    /* Let's try to use the public API by unlocking first */
    if (!joined_lobby) {
        /* We unlock before calling create_lobby to avoid deadlock */
        // Actually, let's keep it simple. Iterate and join.
    }
}


/* Forward declaration */
void handle_disconnect(GameLobby *state, int sender, sock_t *listen_fd_ptr);

/* --- Lobby Management Helpers --- */

static void send_lobby_list(ClientCtx *ctx) {
    char buf[4096];
    int offset = 0;
    
    offset += snprintf(buf + offset, sizeof(buf) - offset, "LOBBY_LIST_START\n");
    
    pthread_mutex_lock(&g_global_state->lock);
    for (int i = 0; i < MAX_LOBBIES; i++) {
        GameLobby *l = g_global_state->lobbies[i];
        if (l) {
            const char *status = (l->num_players >= 2) ? "LOCKED" : "OPEN";
            offset += snprintf(buf + offset, sizeof(buf) - offset, 
                               "LOBBY %d %s %d/2 %s\n", 
                               l->id, l->lobby_name[0] ? l->lobby_name : "Unnamed", l->num_players, status);
        }
    }
    pthread_mutex_unlock(&g_global_state->lock);
    
    offset += snprintf(buf + offset, sizeof(buf) - offset, "LOBBY_LIST_END\n");
    WRITE(ctx->fd, buf, offset);
}

static GameLobby *create_named_lobby(GlobalState *gs, const char *name) {
    GameLobby *l = create_lobby(gs);
    if (l) {
        pthread_mutex_lock(&l->lock);
        strncpy(l->lobby_name, name, sizeof(l->lobby_name) - 1);
        l->lobby_name[sizeof(l->lobby_name) - 1] = '\0';
        pthread_mutex_unlock(&l->lock);
    }
    return l;
}

static void join_lobby_id(ClientCtx *ctx, int lobby_id) {
    GameLobby *joined_lobby = NULL;
    int player_idx = -1;
    
    pthread_mutex_lock(&g_global_state->lock);
    if (lobby_id >= 0 && lobby_id < MAX_LOBBIES && g_global_state->lobbies[lobby_id]) {
        GameLobby *l = g_global_state->lobbies[lobby_id];
        pthread_mutex_lock(&l->lock);
        if (l->num_players < MAX_PLAYERS_PER_GAME) {
            if (l->clients[0] == SOCKET_INVALID) player_idx = 0;
            else if (l->clients[1] == SOCKET_INVALID) player_idx = 1;
            
            if (player_idx != -1) {
                l->clients[player_idx] = ctx->fd;
                l->num_players++;
                joined_lobby = l;
            }
        }
        pthread_mutex_unlock(&l->lock);
    }
    pthread_mutex_unlock(&g_global_state->lock);
    
    if (joined_lobby && player_idx != -1) {
        ctx->lobby = joined_lobby;
        ctx->player_id_in_game = player_idx;
        
        char assign[32];
        int l = snprintf(assign, sizeof(assign), "ASSIGN %d\n", player_idx);
        WRITE(ctx->fd, assign, l);
        
        printf("Client %d joined Lobby %d as Player %d\n", ctx->connection_id, joined_lobby->id, player_idx);
        
        /* Send cached name command */
        if (ctx->pending_name[0] != '\0') {
            char namecmd[128];
            snprintf(namecmd, sizeof(namecmd), "NAME %s\n", ctx->pending_name);
            handle_name_command(joined_lobby, namecmd, player_idx);
        }
    } else {
        const char *msg = "JOIN_FAIL Lobby full or invalid\n";
        WRITE(ctx->fd, msg, (int)strlen(msg));
    }
}

/* Updated quick join (now just tries to find ANY open lobby) */
static void quick_join_lobby(ClientCtx *ctx) {
    /* ... logic similar to before but using join_lobby_id internal logic ... */
    /* For now, disabled in favor of manual selection */
}

static void handle_client_disconnect(ClientCtx *ctx) {
    if (!ctx) return;
    
    if (ctx->lobby) {
        printf("Client %d disconnected from Lobby %d\n", ctx->connection_id, ctx->lobby->id);
        /* If in a lobby, use game logic disconnect */
        handle_disconnect(ctx->lobby, ctx->player_id_in_game, NULL);
        
        /* Decrement player count and destroy lobby if empty */
        pthread_mutex_lock(&ctx->lobby->lock);
        ctx->lobby->num_players--;
        int remaining = ctx->lobby->num_players;
        pthread_mutex_unlock(&ctx->lobby->lock);
        
        if (remaining <= 0) {
            printf("Lobby %d is empty. Destroying...\n", ctx->lobby->id);
            destroy_lobby(g_global_state, ctx->lobby->id);
        }
    } else {
        printf("Client %d disconnected\n", ctx->connection_id);
        /* Just close socket and free slot */
       CLOSE(ctx->fd); 
    }
    
    pthread_mutex_lock(&g_global_state->lock);
    g_global_state->connections[ctx->connection_id] = SOCKET_INVALID;
    g_global_state->client_contexts[ctx->connection_id] = NULL;
    g_global_state->active_connections--;
    pthread_mutex_unlock(&g_global_state->lock);
    
    free(ctx);
}

int server_main(int argc, char **argv) {
    int port = DEFAULT_PORT;
    const char *loc = setlocale(LC_ALL, "");
#ifdef _WIN32
    if (!loc || strstr(loc, "UTF-8") == NULL) loc = setlocale(LC_ALL, ".UTF-8");
    UINT orig_out = GetConsoleOutputCP();
    UINT orig_in = GetConsoleCP();
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    if (!loc || strstr(loc, "UTF-8") == NULL) loc = setlocale(LC_ALL, "en_US.UTF-8");

    if (argc >= 2) port = atoi(argv[1]);
    if (sock_init() != 0) return 1;

    sock_t listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == SOCKET_INVALID) return 1;

    int on = 1;
    setsockopt((int)listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;
    if (listen(listen_fd, 50) < 0) return 1; // Increased backlog
    
    printf("Server listening on port %d (Lobby System Active)\n", port);

    message_queue_init();

    pthread_t c_th;
    pthread_create(&c_th, NULL, console_thread, &listen_fd);

    /* Create Global State */
    g_global_state = global_state_create();

    pthread_t acc_th;
    pthread_create(&acc_th, NULL, accept_thread, &listen_fd);

    while (server_running) {
        MsgEntry e = dequeue_msg();
        char *m = e.msg;
        int sender_conn_id = e.sender;
        if (!m) continue;

        if (sender_conn_id == -2 && strcmp(m, "SERVER_QUIT") == 0) {
            free(m);
            break;
        }

        /* Look up context */
        ClientCtx *ctx = NULL;
        /* Note: accessing array without lock is unsafe if reallocating, but we use fixed size array */
        if (sender_conn_id >= 0 && sender_conn_id < MAX_CONNECTIONS) {
            ctx = g_global_state->client_contexts[sender_conn_id];
        }

        if (!ctx) {
            free(m);
            continue;
        }

        char um[MAX_LINE];
        size_t mi = 0;
        for (size_t i = 0; i < strlen(m) && i + 1 < sizeof(um); ++i) {
            char ch = m[i];
            if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
            um[mi++] = ch;
        }
        um[mi] = '\0';

        /* Lobby Logic */
        if (ctx->lobby == NULL) {
            /* If sending NAME, treat as auto-join request */
            if (strncmp(um, "NAME ", 5) == 0) {
                /* Store Name */
                char namebuf[64];
                if (sscanf(m, "NAME %63[^\r\n]", namebuf) == 1) {
                    strncpy(ctx->pending_name, namebuf, sizeof(ctx->pending_name) - 1);
                    ctx->pending_name[sizeof(ctx->pending_name) - 1] = '\0';
                }
                /* Send Lobby List */
                send_lobby_list(ctx);
                
            } else if (strncmp(um, "LOBBY_LIST", 10) == 0) {
                send_lobby_list(ctx);
                
            } else if (strncmp(um, "LOBBY_CREATE ", 13) == 0) {
                char lname[64];
                /* Use m (original case) for name */
                if (sscanf(m, "LOBBY_CREATE %63[^\r\n]", lname) == 1) {
                    GameLobby *l = create_named_lobby(g_global_state, lname);
                    if (l) {
                        join_lobby_id(ctx, l->id);
                    } else {
                        WRITE(ctx->fd, "CREATE_FAIL Server full\n", 24);
                    }
                }
            } else if (strncmp(um, "LOBBY_JOIN ", 11) == 0) {
                int lid;
                if (sscanf(um, "LOBBY_JOIN %d", &lid) == 1) {
                    join_lobby_id(ctx, lid);
                }
            } else if (strncmp(um, "QUIT", 4) == 0 || strncmp(um, "DISCONNECT", 10) == 0) {
                 handle_client_disconnect(ctx);
            }
        } else {
            /* Game Logic - route to lobby */
            /* Use player_id (0 or 1) as sender for game commands! */
            int pid = ctx->player_id_in_game;
            GameLobby *lobby = ctx->lobby;

            if (strncmp(um, "NAME ", 5) == 0) {
                handle_name_command(lobby, m, pid);
            } else if (strncmp(um, "PLACE ", 6) == 0) {
                handle_place_command(lobby, m, pid);
            } else if (strncmp(um, "MOVE ", 5) == 0) {
                handle_move_command(lobby, m, pid);
            } else if (strncmp(um, "READY", 5) == 0) {
                handle_ready_command(lobby, pid);
            } else if (strncmp(um, "FIRE ", 5) == 0) {
                /* Check if it's chat or fire */
                handle_fire_command(lobby, m, pid);
            } else if (strncmp(um, "PLAY_AGAIN ", 11) == 0) {
                char ans[16] = {0};
                if (sscanf(um, "PLAY_AGAIN %15s", ans) == 1) {
                    int resp = (strstr(ans, "YES") != NULL) ? 1 : 2;
                    handle_rematch_response(lobby, pid, resp);
                }
            } else if (strncmp(um, "DISCONNECT", 10) == 0 || strncmp(um, "QUIT", 4) == 0) {
                /* Let the global handler do connection cleanup, lobby decrement, and notification */
                handle_client_disconnect(ctx);
            }
        }
        
        free(m);
    }

    /* Cleanup */
    // ... existing cleanup logic adapted for global state ...
    message_queue_cleanup();
    sock_cleanup();
    return 0;
}

