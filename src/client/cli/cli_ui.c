/*
 * cli_ui.c - CLI User Interface for Battleship game
 * 
 * Extracted from client_ui.c, provides terminal display
 * and input handling for command-line mode.
 * Called when -cli flag is used or GUI is not available.
 */

#include "client_api.h"
#include "client_state.h"
#include "common.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <stdlib.h>
#endif

#define GRID_ROWS 7
#define GRID_COLS 9

/* ANSI color codes */
#define COL_RESET "\x1b[0m"
#define COL_BOLD "\x1b[1m"
#define COL_RED "\x1b[31m"
#define COL_GREEN "\x1b[32m"
#define COL_YELLOW "\x1b[33m"
#define COL_CYAN "\x1b[36m"

/* Ship display: hardcoded to ASCII 'S' */

void cli_show_grids(void) {
    /* Print two grids side-by-side with headers and borders */
    printf("\n  %sYour grid:%s                          %sOpponent grid:%s\n", COL_BOLD, COL_RESET, COL_BOLD, COL_RESET);

    /* Column headers aligned with row label width */
    printf("     ");
    for (int c = 0; c < GRID_COLS; ++c)
        printf(" %s%c%s ", COL_CYAN, 'A' + c, COL_RESET);
    printf("         ");
    for (int c = 0; c < GRID_COLS; ++c)
        printf(" %s%c%s ", COL_CYAN, 'A' + c, COL_RESET);
    printf("\n");

    /* Rows (display numbered 1..N) */
    for (int r = 0; r < GRID_ROWS; ++r) {
        /* Your grid row with fixed-width row label */
        printf(" %2d |", r + 1);
        for (int c = 0; c < GRID_COLS; ++c) {
            char ch = own_grid[r][c];
            if (ch == '\0' || ch == 0)
                ch = '.';
            /* Display 'S' for any ship (values 1-5) or the old 'S' character */
            if (ch == 'S' || (ch >= 1 && ch <= 5)) {
                printf(" %s%c%s ", COL_GREEN, 'S', COL_RESET);
            } else if (ch == 'H')
                printf(" %s%c%s ", COL_RED, ch, COL_RESET);
            else if (ch == 'M')
                printf(" %s%c%s ", COL_YELLOW, ch, COL_RESET);
            else
                printf(" %c ", ch);
        }
        printf("|   ");

        /* Opponent grid row (same fixed width) */
        printf(" %2d |", r + 1);
        for (int c = 0; c < GRID_COLS; ++c) {
            char ch = opp_grid[r][c];
            if (ch == '\0')
                ch = '.';
            if (ch == 'H')
                printf(" %s%c%s ", COL_RED, ch, COL_RESET);
            else if (ch == 'M')
                printf(" %s%c%s ", COL_YELLOW, ch, COL_RESET);
            else if (ch >= 1 && ch <= 5)
                printf(" %s%c%s ", COL_CYAN, 'S', COL_RESET); /* Revealed ship */
            else
                printf(" %c ", ch);
        }
        printf("|\n");
    }
    printf("\nLegend: %sS%s=Ship %sH%s=Hit %sM%s=Miss .=Unknown\n\n", COL_GREEN, COL_RESET, COL_RED, COL_RESET, COL_YELLOW, COL_RESET);
}

void cli_show_own_grid(void) {
    printf("\n  %sYour grid:%s\n", COL_BOLD, COL_RESET);
    printf("     ");
    for (int c = 0; c < GRID_COLS; ++c)
        printf(" %s%c%s ", COL_CYAN, 'A' + c, COL_RESET);
    printf("\n");

    for (int r = 0; r < GRID_ROWS; ++r) {
        printf(" %2d |", r + 1);
        for (int c = 0; c < GRID_COLS; ++c) {
            char ch = own_grid[r][c];
            if (ch == '\0' || ch == 0)
                ch = '.';
            if (ch == 'S' || (ch >= 1 && ch <= 5)) {
                printf(" %s%c%s ", COL_GREEN, 'S', COL_RESET);
            } else if (ch == 'H')
                printf(" %s%c%s ", COL_RED, ch, COL_RESET);
            else if (ch == 'M')
                printf(" %s%c%s ", COL_YELLOW, ch, COL_RESET);
            else
                printf(" %c ", ch);
        }
        printf("|\n");
    }
    printf("\n");
}

#include "cli_callbacks.h"

/* ==================== CLI Main Entry Point ==================== */

int cli_main(const char *host, int port) {
    /* Setup callbacks to update UI */
    client_set_callbacks(cli_get_callbacks());
    
    /* Connect to server */
    if (client_connect(host, port) != 0) {
        fprintf(stderr, "Failed to connect to server at %s:%d\n", host, port);
        return 1;
    }
    
    printf("Connected to server. Welcome to Battleship!\n");
    printf("Enter your name: ");
    fflush(stdout);
    
    char name[64];
    if (fgets(name, sizeof(name), stdin)) {
        /* Remove trailing newline */
        size_t len = strlen(name);
        if (len > 0 && name[len-1] == '\n') {
            name[len-1] = '\0';
        }
        client_send_name(name);
    }
    
    /* Game loop - integrated with async recv_thread
     * The recv_thread will update state and display as messages arrive
     * Main thread handles CLI input for ship placement and firing
     */
    
    printf("\nPlacing ships...\n");
    int ships_placed = 0;
    
    while (ships_placed < 5) {
        cli_show_own_grid();
        
        printf("Ship %d (length %d):\n", ships_placed + 1, 5 - ships_placed);
        printf("Enter position (row col dir): ");
        fflush(stdout);
        
        char line[64];
        if (!fgets(line, sizeof(line), stdin)) break;
        
        int row, col;
        char dir;
        if (sscanf(line, "%d %d %c", &row, &col, &dir) == 3) {
            if (row >= 0 && row < GRID_ROWS && col >= 0 && col < GRID_COLS) {
                client_send_place(row, col, 5 - ships_placed, dir);
                ships_placed++;
            } else {
                printf("Invalid position!\n");
            }
        }
    }
    
    printf("\nWaiting for opponent to place ships...\n");
    
    /* Main gameplay loop */
    printf("\nGame starting!\n");
    
    while (1) {
        cli_show_grids();
        
        printf("Your turn? Enter target (row col) or q to quit: ");
        fflush(stdout);
        
        char line[64];
        if (!fgets(line, sizeof(line), stdin)) break;
        
        if (line[0] == 'q') break;
        
        int row, col;
        if (sscanf(line, "%d %d", &row, &col) == 2) {
            if (row >= 0 && row < GRID_ROWS && col >= 0 && col < GRID_COLS) {
                client_send_fire(row, col);
            } else {
                printf("Invalid target!\n");
            }
        }
    }
    
    client_disconnect();
    printf("Goodbye!\n");
    return 0;
}
