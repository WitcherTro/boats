#define _GNU_SOURCE
#include "client_commands.h"
#include "client_state.h"
#include "client_ui.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms)*1000)
#endif

#define GRID_ROWS 7
#define GRID_COLS 9

void handle_fire_command(const char *args) {
    char *saveptr = NULL;
    char *args_copy = strdup(args);
    if (!args_copy)
        return;

    char *t1 = strtok_r(args_copy, " \t\n", &saveptr);
    char *t2 = strtok_r(NULL, " \t\n", &saveptr);

    if (!t1 || !t2) {
        printf("Usage: FIRE <row> <col> or FIRE <colLetter> <row>\n");
        free(args_copy);
        return;
    }

    int row = -1, col = -1;

    if ((t1[0] >= 'A' && t1[0] <= 'Z') || (t1[0] >= 'a' && t1[0] <= 'z')) {
        col = (toupper((unsigned char)t1[0]) - 'A');
        row = atoi(t2) - 1;
    } else if ((t2[0] >= 'A' && t2[0] <= 'Z') || (t2[0] >= 'a' && t2[0] <= 'z')) {
        col = (toupper((unsigned char)t2[0]) - 'A');
        row = atoi(t1) - 1;
    } else {
        /* Both numeric: treat first as row, second as col */
        row = atoi(t1) - 1;
        col = atoi(t2) - 1;
    }

    free(args_copy);

    if (row < 0 || row >= GRID_ROWS || col < 0 || col >= GRID_COLS) {
        printf("Coordinates out of range (rows 1..%d, cols A..%c)\n", GRID_ROWS, 'A' + GRID_COLS - 1);
        return;
    }

    char out[64];
    snprintf(out, sizeof(out), "FIRE %d %d\n", row, col);
    WRITE(sockfd, out, strlen(out));
}

void handle_place_command(const char *args) {
    char *saveptr = NULL;
    char *args_copy = strdup(args);
    if (!args_copy)
        return;

    char *toks[4] = {0, 0, 0, 0};
    int ti = 0;
    char *tok = strtok_r(args_copy, " \t\n", &saveptr);
    while (tok && ti < 4) {
        toks[ti++] = tok;
        tok = strtok_r(NULL, " \t\n", &saveptr);
    }

    if (ti < 3) {
        printf("Usage: PLACE <row> <col> <len> [H|V]  (coords may be letter or number in any order)\n");
        free(args_copy);
        return;
    }

    int row = -1, col = -1, lenv = atoi(toks[2]);
    char dirc = 'H';

    if (ti >= 4 && (toks[3][0] == 'V' || toks[3][0] == 'v' || toks[3][0] == 'H' || toks[3][0] == 'h'))
        dirc = toupper((unsigned char)toks[3][0]);

    /* Determine row/col among toks[0] and toks[1] */
    char *a = toks[0], *b = toks[1];

    if ((a[0] >= 'A' && a[0] <= 'Z') || (a[0] >= 'a' && a[0] <= 'z')) {
        col = toupper((unsigned char)a[0]) - 'A';
        row = atoi(b) - 1;
    } else if ((b[0] >= 'A' && b[0] <= 'Z') || (b[0] >= 'a' && b[0] <= 'z')) {
        col = toupper((unsigned char)b[0]) - 'A';
        row = atoi(a) - 1;
    } else {
        row = atoi(a) - 1;
        col = atoi(b) - 1;
    }

    free(args_copy);

    if (row < 0 || row >= GRID_ROWS || col < 0 || col >= GRID_COLS) {
        printf("Placement coordinates out of range (rows 1..%d, cols A..%c)\n", GRID_ROWS, 'A' + GRID_COLS - 1);
        return;
    }

    if (lenv <= 0) {
        printf("Invalid length\n");
        return;
    }

    if (lenv < 2 || lenv > 5) {
        printf("Invalid ship size. Allowed sizes: 2,3,3,4,5\n");
        return;
    }

    if (remaining[lenv] <= 0) {
        printf("No remaining ships of size %d\n", lenv);
        return;
    }

    char out[128];
    snprintf(out, sizeof(out), "PLACE %d %d %d %c\n", row, col, lenv, dirc);
    WRITE(sockfd, out, strlen(out));
}

void handle_random_command(void) {
    /* Build list of sizes that still need placement, based on remaining[] */
    int to_place_count = 0;
    int sizes_to_place[5];
    for (int len = 2; len <= 5; ++len) {
        for (int k = 0; k < remaining[len] && to_place_count < 5; ++k)
            sizes_to_place[to_place_count++] = len;
    }

    if (to_place_count == 0) {
        printf("No ships left to place.\n");
        return;
    }

    struct Placement {
        int r, c, len;
        char dir;
    } placements[5];

    int layout_ok = 0;
    int global_attempt = 0;

    while (!layout_ok && global_attempt < 2000) {
        global_attempt++;
        char tmp[GRID_ROWS][GRID_COLS];
        for (int rr = 0; rr < GRID_ROWS; ++rr)
            for (int cc = 0; cc < GRID_COLS; ++cc)
                tmp[rr][cc] = '.';

        /* Shuffle sizes_to_place */
        for (int si = to_place_count - 1; si > 0; --si) {
            int j = rand() % (si + 1);
            int t = sizes_to_place[si];
            sizes_to_place[si] = sizes_to_place[j];
            sizes_to_place[j] = t;
        }

        int placed_count = 0;
        int fail = 0;

        for (int si = 0; si < to_place_count; ++si) {
            int len = sizes_to_place[si];
            int placed = 0;

            for (int attempt = 0; attempt < 1000 && !placed; ++attempt) {
                char dir = (rand() & 1) ? 'V' : 'H';
                int r = rand() % GRID_ROWS;
                int c = rand() % GRID_COLS;
                int dr = (dir == 'V') ? 1 : 0;
                int dc = (dir == 'H') ? 1 : 0;

                if (r + dr * (len - 1) >= GRID_ROWS)
                    continue;
                if (c + dc * (len - 1) >= GRID_COLS)
                    continue;

                int ok = 1;
                for (int k = 0; k < len; ++k) {
                    int rr = r + k * dr, cc = c + k * dc;
                    if (tmp[rr][cc] != '.') {
                        ok = 0;
                        break;
                    }
                }

                if (!ok)
                    continue;

                /* Place into tmp */
                for (int k = 0; k < len; ++k) {
                    int rr = r + k * dr, cc = c + k * dc;
                    tmp[rr][cc] = 'S';
                }

                placements[placed_count].r = r;
                placements[placed_count].c = c;
                placements[placed_count].len = len;
                placements[placed_count].dir = dir;
                placed_count++;
                placed = 1;
            }

            if (!placed) {
                fail = 1;
                break;
            }
        }

        if (!fail && placed_count == to_place_count) {
            layout_ok = 1;
            break;
        }
    }

    if (!layout_ok) {
        printf("Auto-placement failed after many attempts; try again or place manually.\n");
        return;
    }

    /* Send placements sequentially and wait for confirmation via own_grid update */
    for (int pi = 0; pi < to_place_count; ++pi) {
        int r = placements[pi].r, c = placements[pi].c, len = placements[pi].len;
        char dir = placements[pi].dir;
        int confirmed = 0;
        int tries = 0;

        while (!confirmed && tries < 5) {
            tries++;
            char out[128];
            snprintf(out, sizeof(out), "PLACE %d %d %d %c\n", r, c, len, dir);
            WRITE(sockfd, out, strlen(out));

            /* Wait for server to send PLACED and for recv_thread to update own_grid */
            int wait_ms = 0;
            while (wait_ms < 5000) {
                int all_placed = 1;
                int dr = (dir == 'V' || dir == 'v') ? 1 : 0;
                int dc = (dir == 'H' || dir == 'h') ? 1 : 0;

                for (int k = 0; k < len; ++k) {
                    int rr = r + k * dr, cc = c + k * dc;
                    /* Check if cell has a ship (any value 1-5 or 'S') */
                    if (rr < 0 || rr >= GRID_ROWS || cc < 0 || cc >= GRID_COLS) {
                        all_placed = 0;
                        break;
                    }
                    unsigned char cell = own_grid[rr][cc];
                    if (cell != 'S' && !(cell >= 1 && cell <= 5)) {
                        all_placed = 0;
                        break;
                    }
                }

                if (all_placed) {
                    confirmed = 1;
                    break;
                }

                SLEEP_MS(50);
                wait_ms += 50;
            }
        }

        if (!confirmed) {
            printf("Failed to confirm placement of size %d at %d,%d - aborting auto-placement\n", len, r + 1, c + 1);
            break;
        }
    }
}

void handle_move_command(const char *args) {
    char *saveptr = NULL;
    char *args_copy = strdup(args);
    if (!args_copy)
        return;

    char *token = strtok_r(args_copy, " \t\n", &saveptr);
    if (!token) {
        printf("Usage: MOVE <from_row> <from_col> <to_row> <to_col> [H|V]\n");
        printf("Example: MOVE 1 A 3 D H  or  MOVE A 1 D 3 V\n");
        free(args_copy);
        return;
    }

    int coords[4];
    int is_letter[4];  /* Track if coordinate was a letter (column) or number (row) */
    int coord_count = 0;
    char dir = 'H';

    /* Parse first 4 tokens as coordinates */
    for (int i = 0; i < 4 && token; i++) {
        if (token[0] >= 'A' && token[0] <= 'I') {
            coords[coord_count] = token[0] - 'A';
            is_letter[coord_count] = 1;
            coord_count++;
        } else if (token[0] >= 'a' && token[0] <= 'i') {
            coords[coord_count] = token[0] - 'a';
            is_letter[coord_count] = 1;
            coord_count++;
        } else if (token[0] >= '0' && token[0] <= '9') {
            int val = atoi(token);
            /* Convert 1-based display row to 0-based index */
            if (val >= 1 && val <= 7) {
                coords[coord_count] = val - 1;
            } else {
                coords[coord_count] = val;
            }
            is_letter[coord_count] = 0;
            coord_count++;
        } else {
            printf("Invalid coordinate: %s\n", token);
            free(args_copy);
            return;
        }
        token = strtok_r(NULL, " \t\n", &saveptr);
    }

    if (coord_count != 4) {
        printf("Need 4 coordinates: from_row from_col to_row to_col\n");
        free(args_copy);
        return;
    }
    
    /* Get direction - REQUIRED */
    if (!token) {
        printf("Direction (H or V) is required\n");
        printf("Usage: MOVE <from_row> <from_col> <to_row> <to_col> <H|V>\n");
        free(args_copy);
        return;
    }
    
    if (token[0] == 'V' || token[0] == 'v') {
        dir = 'V';
    } else if (token[0] == 'H' || token[0] == 'h') {
        dir = 'H';
    } else {
        printf("Invalid direction '%s'. Use H (horizontal) or V (vertical)\n", token);
        free(args_copy);
        return;
    }

    /* Determine row/col based on whether token was letter or number */
    int from_row, from_col, to_row, to_col;
    
    /* First two coordinates are "from" position */
    if (is_letter[0] && !is_letter[1]) {
        /* Letter then number = col then row (e.g., "B 1") */
        from_col = coords[0];
        from_row = coords[1];
    } else if (!is_letter[0] && is_letter[1]) {
        /* Number then letter = row then col (e.g., "1 B") */
        from_row = coords[0];
        from_col = coords[1];
    } else {
        /* Both same type, assume row col order */
        from_row = coords[0];
        from_col = coords[1];
    }
    
    /* Next two coordinates are "to" position */
    if (is_letter[2] && !is_letter[3]) {
        /* Letter then number = col then row */
        to_col = coords[2];
        to_row = coords[3];
    } else if (!is_letter[2] && is_letter[3]) {
        /* Number then letter = row then col */
        to_row = coords[2];
        to_col = coords[3];
    } else {
        /* Both same type, assume row col order */
        to_row = coords[2];
        to_col = coords[3];
    }

    /* Send MOVE command to server */
    char movemsg[128];
    int len = snprintf(movemsg, sizeof(movemsg), "MOVE %d %d %d %d %c\n", 
                       from_row, from_col, to_row, to_col, dir);
    WRITE(sockfd, movemsg, len);
    free(args_copy);
}

void handle_ready_command(void) {
    /* Send READY command to server */
    WRITE(sockfd, "READY\n", 6);
    printf("Signaled ready. Waiting for opponent...\n");
}

void handle_help_command(void) {
    printf("\n=== Available Commands ===\n");
    printf("  HELP                   - Show this help message\n");
    printf("  PLACE r c len [H|V]    - Place a ship at row r, column c with length len\n");
    printf("                           (H=horizontal, V=vertical, default: H)\n");
    printf("  MOVE fr fc tr tc [H|V] - Move a ship from (fr,fc) to (tr,tc)\n");
    printf("                           Example: MOVE 0 0 3 4 H\n");
    printf("  RANDOM                 - Automatically place all remaining ships\n");
    printf("                           (Aliases: RAND, AUTO)\n");
    printf("  READY                  - Signal you're ready to start the game\n");
    printf("  FIRE r c               - Fire at row r, column c\n");
    printf("  SHOW                   - Display both grids\n");
    printf("  QUIT                   - Exit the game\n");
    printf("\nShip sizes: 2, 3, 3, 4, 5\n");
    printf("Grid: rows 0-6, columns A-I (or 0-8)\n");
    printf("==========================\n\n");
}
