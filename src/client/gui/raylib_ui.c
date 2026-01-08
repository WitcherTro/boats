/*
 * raylib_ui.c - Raylib-based GUI for Battleship game
 * 
 * Main entry point and high-level game loop.
 */

#include "raylib.h"
#include "client_api.h"
#include "gui_state.h"
#include "gui_draw.h"
#include "gui_input.h"

#include <stdio.h>
#include <string.h>

/* ==================== Callback Handlers ==================== */

static void on_name_received(int player_id, const char *name) {
    if (player_id != ui.my_id) {
        strncpy(ui.opponent_name, name, sizeof(ui.opponent_name) - 1);
        add_message("Opponent connected: %s", name);
    }
}

static void on_placement_start(void) {
    ui.state = STATE_SHIP_PLACEMENT;
    init_ships();
    add_message("Placement phase started");
}

static void on_player_placed(int player_id, int length) {
    if (player_id != ui.my_id) {
        add_message("Opponent placed a ship of length %d", length);
    }
}

static void on_game_start(void) {
    ui.state = STATE_PLAYING;
    add_message("Game started!");
}

static void on_turn_change(int player_id) {
    current_turn = player_id;
    if (player_id == ui.my_id) {
        add_message("Your turn!");
    } else {
        add_message("Opponent's turn...");
    }
}

static void on_fire_result(int row, int col, int hit) {
    if (hit) {
        opp_grid[row][col] = 'H';
        add_message("HIT at %c%d!", 'A' + col, row);
    } else {
        opp_grid[row][col] = 'M';
        add_message("Miss at %c%d", 'A' + col, row);
    }
}

static void on_opponent_fire(int row, int col, int hit) {
    if (hit) {
        add_message("Opponent HIT your ship at %c%d!", 'A' + col, row);
    } else {
        add_message("Opponent missed at %c%d", 'A' + col, row);
    }
}

static void on_ship_sunk(int player_id, int length) {
    if (player_id == ui.my_id) {
        add_message("*** YOUR SHIP SUNK! ***");
    } else {
        add_message("*** OPPONENT'S SHIP SUNK! ***");
    }
}

static void on_game_end(int winner_id) {
    ui.state = STATE_GAME_OVER;
    ui.winner_id = winner_id;
    
    if (winner_id == ui.my_id) {
        add_message("YOU WIN! Press Y for rematch or N to quit");
    } else {
        add_message("YOU LOSE! Press Y for rematch or N to quit");
    }
}

static void on_game_reset(void) {
    ui.state = STATE_SHIP_PLACEMENT;
    ui.ships_placed = 0;
    ui.selected_row = -1;
    ui.selected_col = -1;
    ui.placement_confirmed = 0;
    ui.auto_placing = 0;
    ui.auto_place_ship_idx = 0;
    ui.auto_place_attempts = 0;
    ui.auto_place_last_placed = 0;
    init_ships();
    add_message("Game restarted!");
}

static void on_opponent_disconnected(void) {
    ui.state = STATE_WAITING_PLACEMENT;
    ui.ships_placed = 0;
    ui.selected_row = -1;
    ui.selected_col = -1;
    ui.placement_confirmed = 0;
    ui.auto_placing = 0;
    ui.auto_place_ship_idx = 0;
    ui.auto_place_attempts = 0;
    ui.auto_place_last_placed = 0;
    init_ships();
    add_message("Opponent disconnected. Waiting for new player...");
}

static void on_message(const char *msg) {
    add_message("%s", msg);
}

static void on_grid_update(void) {
    /* Grid is updated automatically */
}

/* ==================== Main GUI Loop ==================== */

int gui_main(const char *host, int port) {
    ClientCallbacks callbacks = {
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
    
    client_set_callbacks(&callbacks);
    
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Battleship - raylib GUI");
    SetTargetFPS(FPS);
    
    if (host == NULL) {
        ui.state = STATE_CONNECT_INPUT;
        add_message("Enter Server IP and Port");
    } else {
        if (client_connect(host, port) != 0) {
            fprintf(stderr, "Failed to connect to server\n");
            CloseWindow();
            return 1;
        }
        ui.state = STATE_NAME_INPUT;
        ui.my_id = client_get_my_id();
        add_message("Connected! Enter your name and press ENTER");
    }
    
    while (!WindowShouldClose()) {
        if (server_disconnected) {
            add_message("Server disconnected. Closing...");
            // Give it a moment to render the message or just break
            // For now, let's just break to exit cleanly
            break; 
        }

        /* Sync state with network layer */
        ui.ships_placed = placed_count;
        
        int new_id = client_get_my_id();
        if (new_id >= 0 && ui.my_id != new_id) {
            ui.my_id = new_id;
            add_message("Assigned ID: %d", ui.my_id);
        }
        
        switch (ui.state) {
            case STATE_CONNECT_INPUT:
                handle_connect_input();
                break;
            case STATE_NAME_INPUT:
                handle_name_input();
                break;
            case STATE_SHIP_PLACEMENT:
                handle_ship_placement();
                break;
            case STATE_PLAYING:
                handle_gameplay();
                break;
            case STATE_GAME_OVER:
                handle_game_over();
                break;
            default:
                break;
        }
        
        for (int i = 0; i < msg_count; i++) {
            messages[i].fade_time -= GetFrameTime();
        }
        
        BeginDrawing();
        ClearBackground(COLOR_BG);
        
        draw_ui_info();
        
        draw_grid(GRID_OFFSET_X, GRID_OFFSET_Y, own_grid, 1);
        
        // Dynamic opponent grid position
        int opp_offset = (ui.state == STATE_SHIP_PLACEMENT) ? 400 : 100;
        int right_grid_x = GRID_OFFSET_X + (GRID_COLS * (GRID_CELL_SIZE + GRID_SPACING)) + opp_offset;
        
        draw_grid(right_grid_x, GRID_OFFSET_Y, opp_grid, 0);
        
        draw_ships();
        
        DrawTextBold("Your Grid", GRID_OFFSET_X + 50, GRID_OFFSET_Y - 70, 24, COLOR_TEXT);
        DrawTextBold("Opponent Grid", right_grid_x + 50, GRID_OFFSET_Y - 70, 24, COLOR_TEXT);
        
        int info_y = 540;
        switch (ui.state) {
            case STATE_CONNECT_INPUT: {
                DrawTextBold("Server Connection", 60, info_y, 24, COLOR_TEXT);
                
                // IP Field
                DrawTextBold("IP Address:", 60, info_y + 35, 20, COLOR_TEXT);
                DrawRectangle(180, info_y + 30, 200, 30, WHITE);
                DrawRectangleLines(180, info_y + 30, 200, 30, (ui.active_input_field == 0) ? BLUE : BLACK);
                DrawText(ui.server_ip, 185, info_y + 35, 20, BLACK);
                
                // Port Field
                DrawTextBold("Port:", 400, info_y + 35, 20, COLOR_TEXT);
                DrawRectangle(460, info_y + 30, 100, 30, WHITE);
                DrawRectangleLines(460, info_y + 30, 100, 30, (ui.active_input_field == 1) ? BLUE : BLACK);
                DrawText(ui.server_port_str, 465, info_y + 35, 20, BLACK);
                
                DrawTextBold("Press TAB to switch | ENTER to connect", 60, info_y + 70, 18, GRAY);
                break;
            }

            case STATE_CONNECTING:
                DrawTextBold("Connecting...", 60, info_y, 32, COLOR_TEXT);
                break;
                
            case STATE_NAME_INPUT: {
                DrawTextBold("Enter name:", 60, info_y, 24, COLOR_TEXT);
                DrawRectangle(60, info_y + 35, 280, 45, WHITE);
                DrawRectangleLines(60, info_y + 35, 280, 45, COLOR_TEXT);
                DrawTextBold(ui.player_name, 70, info_y + 40, 24, COLOR_TEXT);
                DrawTextBold("Press ENTER", 60, info_y + 85, 18, (Color){80, 80, 80, 255});
                break;
            }
            
            case STATE_WAITING_PLACEMENT:
                DrawTextBold("Waiting for opponent to join...", 60, info_y, 28, COLOR_TEXT);
                break;
                
            case STATE_SHIP_PLACEMENT: {
                DrawTextBold("Place ships", 60, info_y, 26, COLOR_TEXT);
                DrawTextBold("Drag ships from dock to grid", 60, info_y + 32, 20, COLOR_TEXT);
                DrawTextBold("Press R to rotate | Press A for Auto-Place", 60, info_y + 58, 18, (Color){80, 80, 80, 255});
                break;
            }
                
            case STATE_WAITING_OPPONENT:
                DrawTextBold("Waiting for opponent...", 60, info_y, 28, COLOR_TEXT);
                break;
                
            case STATE_PLAYING: {
                if (current_turn == ui.my_id) {
                    DrawTextBold("YOUR TURN - Click opponent grid to fire", 60, info_y, 22, (Color){0, 180, 0, 255});
                } else {
                    DrawTextBold("Opponent's turn...", 60, info_y, 22, (Color){180, 150, 0, 255});
                }
                break;
            }
                
            case STATE_GAME_OVER: {
                if (ui.winner_id == ui.my_id) {
                    DrawTextBold("YOU WIN!", 60, info_y, 44, (Color){0, 200, 0, 255});
                } else {
                    DrawTextBold("YOU LOSE!", 60, info_y, 44, (Color){220, 0, 0, 255});
                }
                DrawTextBold("Y for rematch | N to quit", 60, info_y + 50, 20, COLOR_TEXT);
                break;
            }
        }
        
        draw_messages();
        
        EndDrawing();
    }
    
    CloseWindow();
    client_disconnect();
    return 0;
}
