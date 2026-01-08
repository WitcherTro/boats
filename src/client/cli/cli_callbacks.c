#include "cli_callbacks.h"
#include "client_ui.h" /* For show_grids */
#include "client_state.h" /* For player names, etc */
#include <stdio.h>

/* ANSI color codes */
#define COL_RESET "\x1b[0m"
#define COL_BOLD "\x1b[1m"
#define COL_RED "\x1b[31m"
#define COL_GREEN "\x1b[32m"
#define COL_YELLOW "\x1b[33m"
#define COL_MAGENTA "\x1b[35m"

static void on_name_received(int player_id, const char *name) {
    if (player_id == my_id) {
        printf("You are known as ");
        print_utf8(name);
        printf("\n");
    } else {
        printf("Player %d is ", player_id);
        print_utf8(name);
        printf("\n");
    }
}

static void on_grid_update(void) {
    /* Optional: Don't redraw on every update, only on specific events */
}

static void on_placement_start(void) {
    printf("\n%sPlace your ships now using the PLACE command:%s\n", COL_BOLD, COL_RESET);
    printf("  Usage: PLACE <row|col> <col|row> <length> [H|V]  (coords can be letter or number in any order)\n");
    printf("  Ships to place: 2, 3, 3, 4, 5  -- you MUST place all of them before the game starts.\n\n");
    printf("  Tip: you may type RANDOM (or RAND/AUTO) to auto-place all ships for you.\n");
    printf("  After placing, use MOVE to reposition ships before typing READY.\n\n");
    printf("Example: PLACE D 3 4 H   (or: PLACE 3 D 4 H)\n");
    printf("Example: MOVE A 1 B 1 V  (moves ship from A1 to B1 vertically)\n\n");
    printf("%sShips to place:%s\n", COL_BOLD, COL_RESET);
    printf("  1 ship of length 2\n");
    printf("  2 ships of length 3\n");
    printf("  1 ship of length 4\n");
    printf("  1 ship of length 5\n\n");
}

static void on_player_placed(int player_id, int length) {
    if (player_id == my_id)
        printf("%sYou placed a %d-long ship%s\n", COL_GREEN, length, COL_RESET);
    else {
        if (player_names[player_id][0]) {
            printf("%s", COL_MAGENTA);
            print_utf8(player_names[player_id]);
            printf(" placed a %d-long ship%s\n", length, COL_RESET);
        } else
            printf("%sPlayer %d placed a %d-long ship%s\n", COL_MAGENTA, player_id, length, COL_RESET);
    }
}

static void on_game_start(void) {
    printf("%sFiring commands and rules:%s\n", COL_BOLD, COL_RESET);
    printf("  Use: FIRE <row> <col>  or  FIRE <colLetter> <row>  (coords may be letter or number in any order)\n");
    printf("  Example: FIRE D 3   or   FIRE 3 D\n");
    printf("  Rules: If your shot is a HIT you get another turn; on a MISS the opponent gets the turn.\n\n");
    show_grids();
}

static void on_turn_change(int player_id) {
    printf("\n");
    if (player_id == my_id) {
        printf("%sYour turn%s\n", COL_YELLOW, COL_RESET);
    } else if (player_names[player_id][0]) {
        printf("%s", COL_MAGENTA);
        print_utf8(player_names[player_id]);
        printf("'s turn%s\n", COL_RESET);
    } else {
        printf("%sPlayer %d's turn%s\n", COL_MAGENTA, player_id, COL_RESET);
    }
}

static void on_fire_result(int row, int col, int hit) {
    if (hit) {
        printf("Your fire at %d,%d -> %s%s%s\n", row + 1, col + 1, COL_GREEN, "HIT", COL_RESET);
    } else {
        printf("Your fire at %d,%d -> %s%s%s\n", row + 1, col + 1, COL_YELLOW, "MISS", COL_RESET);
    }
    show_grids();
    if (!hit) {
        printf("\n");
    }
}

static void on_opponent_fire(int row, int col, int hit) {
    if (hit) {
        printf("You were shot at %d,%d -> %s%s%s\n", row + 1, col + 1, COL_RED, "HIT", COL_RESET);
    } else {
        printf("You were shot at %d,%d -> %s%s%s\n", row + 1, col + 1, COL_YELLOW, "MISS", COL_RESET);
    }
    show_grids();
}

static void on_ship_sunk(int player_id, int length) {
    printf("\n%s", COL_BOLD);
    if (player_id == my_id) {
        printf("%s[SHIP SUNK] Your %d-long ship was destroyed.%s\n", COL_RED, length, COL_RESET);
    } else {
        printf("%s[SHIP SUNK] You sank their %d-long ship.%s\n", COL_GREEN, length, COL_RESET);
    }
    printf("%s\n", COL_RESET);
}

static void on_game_end(int winner_id) {
    if (winner_id == my_id)
        printf("You win!\n");
    else if (player_names[winner_id][0]) {
        print_utf8(player_names[winner_id]);
        printf(" wins\n");
    } else
        printf("Player %d wins\n", winner_id);
    
    printf("Play again? Type Y or N and press Enter.\n");
}

static void on_game_reset(void) {
    printf("Game restarted. Place your ships.\n");
}

static void on_opponent_disconnected(void) {
    printf("Opponent disconnected. Waiting for new player...\n");
}

static void on_message(const char *msg) {
    printf("%s\n", msg);
}

static ClientCallbacks callbacks = {
    .on_name_received = on_name_received,
    .on_grid_update = on_grid_update,
    .on_placement_start = on_placement_start,
    .on_player_placed = on_player_placed,
    .on_game_start = on_game_start,
    .on_turn_change = on_turn_change,
    .on_fire_result = on_fire_result,
    .on_opponent_fire = on_opponent_fire,
    .on_ship_sunk = on_ship_sunk,
    .on_game_end = on_game_end,
    .on_game_reset = on_game_reset,
    .on_opponent_disconnected = on_opponent_disconnected,
    .on_message = on_message
};

ClientCallbacks* cli_get_callbacks(void) {
    return &callbacks;
}
