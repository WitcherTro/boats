#include "client_ui.h"
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

/* Ship display: 'S' for ships, 'H' for hits, 'M' for misses */

void show_grids(void) {
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
