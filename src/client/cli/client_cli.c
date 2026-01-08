#define _POSIX_C_SOURCE 200112L
#include "common.h"
#include "game.h"
#include "client_state.h"
#include "client_ui.h"
#include "client_recv.h"
#include "client_commands.h"
#include "client_api.h"
#include "cli_callbacks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <locale.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms)*1000)
#endif

int console_utf8_capable = 0;

/* Read a line from stdin and ensure UTF-8 output, converting from the active Windows codepage if needed */
static int read_line_utf8(char *out, size_t cap) {
#ifdef _WIN32
    char raw[512];
    if (!fgets(raw, sizeof(raw), stdin)) return 0;

    size_t ln = strlen(raw);
    if (ln && raw[ln - 1] == '\n') raw[--ln] = '\0';

    UINT cp = GetConsoleCP();
    if (cp != CP_UTF8) {
        wchar_t wbuf[512];
        int wlen = MultiByteToWideChar(cp, 0, raw, -1, wbuf, (int)(sizeof(wbuf) / sizeof(wbuf[0])));
        if (wlen > 0) {
            int outlen = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, out, (int)cap, NULL, NULL);
            if (outlen > 0) {
                out[cap - 1] = '\0';
                return 1;
            }
        }
    }

    strncpy(out, raw, cap - 1);
    out[cap - 1] = '\0';
    return 1;
#else
    if (!fgets(out, cap, stdin)) return 0;
    size_t ln = strlen(out);
    if (ln && out[ln - 1] == '\n') out[--ln] = '\0';
    return 1;
#endif
}

int client_run(const char *host, int port) {
    /* Register CLI callbacks for output */
    client_set_callbacks(cli_get_callbacks());

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
    /* Enable UTF-8 output and ANSI escape processing on Windows consoles */
    /* First try to set output/input codepages to UTF-8 */
    UINT orig_out = GetConsoleOutputCP();
    UINT orig_in = GetConsoleCP();
    
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    
    UINT out_after = GetConsoleOutputCP();
    UINT in_after = GetConsoleCP();
    
    /* If codepage didn't stick, try enabling VT processing and fallback */
    int utf8_available = (out_after == CP_UTF8 && in_after == CP_UTF8);
    
    if (!utf8_available) {
        /* UTF-8 console unavailable, will use ASCII 'S' for ships */
        /* Restore original codepages and try VT-only mode */
        SetConsoleOutputCP(orig_out);
        SetConsoleCP(orig_in);
    } else {
        console_utf8_capable = 1;
    }
    
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, mode);
        }
    }
    
    if (hIn != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hIn, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
            SetConsoleMode(hIn, mode);
        }
    }
#endif

    /* Seed RNG for random placement */
    srand((unsigned)time(NULL));

    if (sock_init() != 0) {
        fprintf(stderr, "Winsock init failed\n");
        return 1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == SOCKET_INVALID) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in srv = {0};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    inet_pton(AF_INET, host, &srv.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
        perror("connect");
        return 1;
    }
    printf("Connected to %s:%d\n", host, port);

    /* Read possible ASSIGN message from server */
    char tmp[MAX_LINE];
    ssize_t rn = read_line(sockfd, tmp, sizeof(tmp));
    if (rn > 0) {
        if (sscanf(tmp, "ASSIGN %d", &my_id) == 1) {
            printf("Assigned id %d\n", my_id);
            init_grids();

            /* Send name to server */
            char namebuf[64] = "";
            
            /* Prompt for name */
            printf("Enter your name: ");
            fflush(stdout);
            if (!read_line_utf8(namebuf, sizeof(namebuf)))
                namebuf[0] = '\0';

            if (namebuf[0]) {
                char nm[128];
                int l = snprintf(nm, sizeof(nm), "NAME %s\n", namebuf);
                WRITE(sockfd, nm, l);
            }
        } else {
            /* If it's not ASSIGN, print and let receiver thread handle further messages */
            printf("[server] %s", tmp);
        }
    }

    printf("Type 'help' for a list of available commands.\n");

    pthread_t t;
    pthread_create(&t, NULL, recv_thread, NULL);

    /* Read lines from stdin and send. Supports UTF-8 input. */
    char line[512];
    while (read_line_utf8(line, sizeof(line))) {
        if (server_disconnected) break;

        /* Trim leading spaces */
        char *p = line;
        while (*p && (*p == ' ' || *p == '\t'))
            p++;
        if (*p == '\0')
            continue;

        /* Case-insensitive command check */
        char cmd[8] = {0};
        int i = 0;
        while (p[i] && p[i] != ' ' && p[i] != '\t' && p[i] != '\n' && i < (int)sizeof(cmd) - 1) {
            char ch = p[i];
            if (ch >= 'a' && ch <= 'z')
                ch = ch - 'a' + 'A';
            cmd[i] = ch;
            i++;
        }

        if (waiting_rematch) {
            /* Interpret this input as rematch answer */
            char ans = p[0];
            while (ans == ' ' || ans == '\t') {
                p++;
                ans = *p;
            }
            if (ans == 'Y' || ans == 'y') {
                WRITE(sockfd, "PLAY_AGAIN YES\n", 16);
            } else {
                WRITE(sockfd, "PLAY_AGAIN NO\n", 15);
            }
            waiting_rematch = 0;
            awaiting_restart = 1;
            printf("Waiting for opponent...\n");
            continue;
        }

        if (awaiting_restart) {
            printf("Waiting for game restart from server...\n");
            continue;
        }

        if (strcmp(cmd, "QUIT") == 0 || strcmp(cmd, "EXIT") == 0)
            break;

        if (strcmp(cmd, "SHOW") == 0) {
            show_grids();
            continue;
        }

        if (strcmp(cmd, "DEBUG") == 0) {
            /* Debug: show actual cell values */
            printf("\nDEBUG - Cell values (0=empty, 1-5=ship IDs, H=hit, M=miss):\n");
            for (int r = 0; r < GRID_ROWS; r++) {
                printf("  %d | ", r + 1);
                for (int c = 0; c < GRID_COLS; c++) {
                    unsigned char val = own_grid[r][c];
                    if (val == 0 || val == '.') printf(".  ");
                    else if (val >= 1 && val <= 5) printf("%d  ", val);
                    else printf("%c  ", val);
                }
                printf("\n");
            }
            printf("Placed count: %d, Ship lengths: [%d,%d,%d,%d,%d,%d]\n\n",
                   placed_count, ship_lengths[0], ship_lengths[1], ship_lengths[2],
                   ship_lengths[3], ship_lengths[4], ship_lengths[5]);
            continue;
        }

        if (strcmp(cmd, "RANDOM") == 0 || strcmp(cmd, "RAND") == 0 || strcmp(cmd, "AUTO") == 0) {
            handle_random_command();
            continue;
        }

        if (strcmp(cmd, "FIRE") == 0) {
            char *args = p + i;
            while (*args == ' ' || *args == '\t')
                args++;
            handle_fire_command(args);
            continue;
        }

        if (strcmp(cmd, "PLACE") == 0) {
            char *args = p + i;
            while (*args == ' ' || *args == '\t')
                args++;
            handle_place_command(args);
            continue;
        }

        if (strcmp(cmd, "MOVE") == 0) {
            char *args = p + i;
            while (*args == ' ' || *args == '\t')
                args++;
            handle_move_command(args);
            continue;
        }

        if (strcmp(cmd, "READY") == 0) {
            handle_ready_command();
            continue;
        }

        if (strcmp(cmd, "HELP") == 0) {
            handle_help_command();
            continue;
        }

        printf("Unknown command: %s\n", cmd);
        printf("Type 'help' for a list of available commands.\n");
    }

    client_running = 0;
    shutdown(sockfd, SHUT_RDWR_FLAG);
    CLOSE(sockfd);
    pthread_join(t, NULL);
    sock_cleanup();
    return 0;
}
