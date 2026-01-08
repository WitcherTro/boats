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
            /* If accept fails but server running, maybe just retry or log */
            continue;
        }
        
        pthread_mutex_lock(&g_server_state->lock);
        
        int assigned = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (g_server_state->clients[i] == SOCKET_INVALID) {
                g_server_state->clients[i] = c;
                assigned = i;
                break;
            }
        }
        
        if (assigned != -1) {
            /* Send assignment message */
            char assign[32];
            int l = snprintf(assign, sizeof(assign), "ASSIGN %d\n", assigned);
            WRITE(c, assign, l);
            
            /* Spawn reader thread */
            ClientCtx *ctx = malloc(sizeof(ClientCtx));
            ctx->fd = c;
            ctx->id = assigned;
            
            g_server_state->active_threads++;
            
            pthread_t th;
            pthread_create(&th, NULL, client_reader, ctx);
            pthread_detach(th); /* Detach so we don't need to join */
            
            printf("Client %d connected\n", assigned);
        } else {
            /* Server full */
            const char *msg = "BUSY Game already in progress\n";
            WRITE(c, msg, (int)strlen(msg));
            shutdown(c, SHUT_RDWR_FLAG);
            CLOSE(c);
        }
        
        pthread_mutex_unlock(&g_server_state->lock);
    }
    return NULL;
}

int server_main(int argc, char **argv) {
    int port = DEFAULT_PORT;
    /* Ensure proper locale for UTF-8 handling (force UTF-8 where available) */
    const char *loc = setlocale(LC_ALL, "");
#ifdef _WIN32
    if (!loc || strstr(loc, "UTF-8") == NULL)
        loc = setlocale(LC_ALL, ".UTF-8"); /* Windows-specific UTF-8 locale */
#endif
    if (!loc || strstr(loc, "UTF-8") == NULL)
        loc = setlocale(LC_ALL, "en_US.UTF-8");
    if (!loc || strstr(loc, "UTF-8") == NULL)
        loc = setlocale(LC_ALL, "C.UTF-8");
    if (!loc || strstr(loc, "UTF-8") == NULL)
        loc = setlocale(LC_ALL, "sk_SK.UTF-8");

#ifdef _WIN32
    /* Enable UTF-8 output on Windows server console */
    UINT orig_out = GetConsoleOutputCP();
    UINT orig_in = GetConsoleCP();
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    UINT out_after = GetConsoleOutputCP();
    UINT in_after = GetConsoleCP();
    
    if (out_after != CP_UTF8 || in_after != CP_UTF8) {
        /* UTF-8 console not available, but server continues normally */
        /* (only client needs to fallback to ASCII ships) */
        SetConsoleOutputCP(orig_out);
        SetConsoleCP(orig_in);
    }
#endif
    if (argc >= 2) port = atoi(argv[1]);
    if (sock_init() != 0) {
        fprintf(stderr, "Winsock init failed\n");
        return 1;
    }

    sock_t listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == SOCKET_INVALID) {
        perror("socket");
        return 1;
    }
    int on = 1;
    setsockopt((int)listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    if (listen(listen_fd, 4) < 0) {
        perror("listen");
        return 1;
    }
    printf("Server listening on port %d\n", port);

    /* Initialize message queue */
    message_queue_init();

    /* Start console thread */
    pthread_t c_th;
    pthread_create(&c_th, NULL, console_thread, &listen_fd);

    /* Create server state */
    ServerState *state = server_state_create();
    if (!state) {
        fprintf(stderr, "Failed to create server state\n");
        return 1;
    }
    g_server_state = state;

    /* Start accept thread */
    pthread_t acc_th;
    pthread_create(&acc_th, NULL, accept_thread, &listen_fd);

    /* Main message loop */
    while (server_running) {
        MsgEntry e = dequeue_msg();
        char *m = e.msg;
        int sender = e.sender;
        if (!m) continue;

        if (sender == -2 && strcmp(m, "SERVER_QUIT") == 0) {
            free(m);
            break;
        }

        /* Uppercase copy for command parsing (preserve case for content) */
        char um[MAX_LINE];
        size_t mi = 0;
        for (size_t i = 0; i < strlen(m) && i + 1 < sizeof(um); ++i) {
            char ch = m[i];
            if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
            um[mi++] = ch;
        }
        um[mi] = '\0';

        /* Handle different commands */
        if (strncmp(um, "NAME ", 5) == 0) {
            handle_name_command(state, m, sender);
        } else if (strncmp(um, "PLACE ", 6) == 0) {
            handle_place_command(state, m, sender);
        } else if (strncmp(um, "MOVE ", 5) == 0) {
            handle_move_command(state, m, sender);
        } else if (strncmp(um, "READY", 5) == 0) {
            handle_ready_command(state, sender);
        } else if (strncmp(um, "FIRE ", 5) == 0) {
            handle_fire_command(state, m, sender);
        } else if (strncmp(um, "PLAY_AGAIN ", 11) == 0) {
            char ans[16] = {0};
            if (sscanf(um, "PLAY_AGAIN %15s", ans) == 1) {
                int resp = (strstr(ans, "YES") != NULL) ? 1 : 2;
                handle_rematch_response(state, sender, resp);
            }
        } else if (strncmp(um, "DISCONNECT", 10) == 0) {
            handle_disconnect(state, sender, &listen_fd);
        } else if (strncmp(um, "QUIT", 4) == 0) {
            /* Treat QUIT as disconnect */
            handle_disconnect(state, sender, &listen_fd);
        }
        
        free(m);
    }

    /* Join threads to prevent memory leaks */
    pthread_join(c_th, NULL);
    pthread_join(acc_th, NULL);

    /* Wait for detached client threads to clean up */
    printf("Waiting for client threads to finish...\n");
    while (1) {
        pthread_mutex_lock(&state->lock);
        int count = state->active_threads;
        pthread_mutex_unlock(&state->lock);
        
        if (count <= 0) break;
        
        #ifdef _WIN32
        Sleep(10);
        #else
        struct timespec ts = {0, 10000000}; /* 10ms */
        nanosleep(&ts, NULL);
        #endif
    }

    /* Cleanup */
    if (listen_fd != SOCKET_INVALID) {
        CLOSE(listen_fd);
    }
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (state->clients[i] != SOCKET_INVALID) {
            CLOSE(state->clients[i]);
        }
    }
    server_state_destroy(state);
    message_queue_cleanup();
    sock_cleanup();
    return 0;
}
