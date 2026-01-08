#include "gui_input.h"
#include "client_api.h"
#include <stdlib.h>
#include <string.h>

void handle_connect_input(void) {
    // Mouse click handling
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        int mx = GetMouseX();
        int my = GetMouseY();
        int info_y = 540;
        
        // IP Field: x=180, y=info_y+30, w=200, h=30
        if (mx >= 180 && mx <= 380 && my >= info_y + 30 && my <= info_y + 60) {
            ui.active_input_field = 0;
        }
        
        // Port Field: x=460, y=info_y+30, w=100, h=30
        if (mx >= 460 && mx <= 560 && my >= info_y + 30 && my <= info_y + 60) {
            ui.active_input_field = 1;
        }
    }

    // Tab to switch fields
    if (IsKeyPressed(KEY_TAB)) {
        ui.active_input_field = !ui.active_input_field;
    }
    
    // Handle text input
    int key = GetCharPressed();
    while (key > 0) {
        if ((key >= 32 && key <= 126)) {
            if (ui.active_input_field == 0 && ui.ip_input_len < 63) {
                ui.server_ip[ui.ip_input_len++] = (char)key;
                ui.server_ip[ui.ip_input_len] = '\0';
            } else if (ui.active_input_field == 1 && ui.port_input_len < 15) {
                if (key >= '0' && key <= '9') { // Only digits for port
                    ui.server_port_str[ui.port_input_len++] = (char)key;
                    ui.server_port_str[ui.port_input_len] = '\0';
                }
            }
        }
        key = GetCharPressed();
    }
    
    if (IsKeyPressed(KEY_BACKSPACE)) {
        if (ui.active_input_field == 0 && ui.ip_input_len > 0) {
            ui.ip_input_len--;
            ui.server_ip[ui.ip_input_len] = '\0';
        } else if (ui.active_input_field == 1 && ui.port_input_len > 0) {
            ui.port_input_len--;
            ui.server_port_str[ui.port_input_len] = '\0';
        }
    }
    
    if (IsKeyPressed(KEY_ENTER)) {
        if (ui.ip_input_len > 0 && ui.port_input_len > 0) {
            ui.state = STATE_CONNECTING;
            add_message("Connecting to %s:%s...", ui.server_ip, ui.server_port_str);
            
            if (client_connect(ui.server_ip, atoi(ui.server_port_str)) != 0) {
                ui.state = STATE_CONNECT_INPUT;
                add_message("Failed to connect! Check IP/Port.");
            } else {
                ui.state = STATE_NAME_INPUT;
                ui.my_id = client_get_my_id();
                add_message("Connected! Enter your name.");
            }
        }
    }
}

void handle_name_input(void) {
    int key = GetCharPressed();
    while (key > 0) {
        if (key >= 32 && key <= 126 && ui.name_input_len < 63) {
            ui.player_name[ui.name_input_len++] = (char)key;
            ui.player_name[ui.name_input_len] = '\0';
        }
        key = GetCharPressed();
    }
    
    if (IsKeyPressed(KEY_BACKSPACE) && ui.name_input_len > 0) {
        ui.name_input_len--;
        ui.player_name[ui.name_input_len] = '\0';
    }
    
    if (IsKeyPressed(KEY_ENTER) && ui.name_input_len > 0) {
        client_send_name(ui.player_name);
        ui.state = STATE_WAITING_PLACEMENT;
        add_message("Name sent! Waiting for opponent...");
    }
}

void handle_ship_placement(void) {
    static int dragged_ship_idx = -1;
    
    // Auto-placement trigger
    if (IsKeyPressed(KEY_A)) {
        // Reset all ships first
        for (int i = 0; i < 5; i++) {
            ui.ships[i].is_placed = false;
        }

        for (int i = 0; i < 5; i++) {
            DraggableShip *ship = &ui.ships[i];
            int attempts = 0;
            while (attempts < 1000) {
                int r = rand() % GRID_ROWS;
                int c = rand() % GRID_COLS;
                char dir = (rand() % 2) ? 'H' : 'V';
                
                // Check bounds
                int end_row = r;
                int end_col = c;
                if (dir == 'H') end_col += ship->length - 1;
                else end_row += ship->length - 1;
                
                if (end_row >= GRID_ROWS || end_col >= GRID_COLS) {
                    attempts++;
                    continue;
                }
                
                // Check overlap with ALREADY PLACED ships (indices 0 to i-1)
                int overlap = 0;
                for (int j = 0; j < i; j++) {
                    DraggableShip *other = &ui.ships[j];
                    // Calculate other ship bounds
                    int other_end_r = other->row;
                    int other_end_c = other->col;
                    if (other->direction == 'H') other_end_c += other->length - 1;
                    else other_end_r += other->length - 1;
                    
                    // Check intersection
                    if (r <= other_end_r && end_row >= other->row && 
                        c <= other_end_c && end_col >= other->col) {
                        overlap = 1;
                        break;
                    }
                }
                
                if (!overlap) {
                    ship->row = r;
                    ship->col = c;
                    ship->direction = dir;
                    ship->is_placed = true;
                    break; // Move to next ship
                }
                attempts++;
            }
        }
        add_message("Ships auto-placed! Drag to adjust.");
    }
    
    // Handle dragging start
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        int mx = GetMouseX();
        int my = GetMouseY();
        
        // Check "Confirm" button
        int placed_cnt = 0;
        for(int i=0; i<5; i++) if(ui.ships[i].is_placed) placed_cnt++;
        
        if (placed_cnt == 5 && mx >= 600 && mx <= 800 && my >= 500 && my <= 550) {
            add_message("Sending ship placements...");
            // Send all ships
            for (int i = 0; i < 5; i++) {
                client_send_place(ui.ships[i].row, ui.ships[i].col, ui.ships[i].length, ui.ships[i].direction);
            }
            client_send_ready();
            ui.state = STATE_WAITING_OPPONENT;
            add_message("Ships placed! Waiting for opponent...");
            return;
        }

        // Check collision with ships
        for (int i = 0; i < 5; i++) {
            DraggableShip *ship = &ui.ships[i];
            int x, y, w, h;
            
            if (ship->is_placed) {
                x = GRID_OFFSET_X + ship->col * (GRID_CELL_SIZE + GRID_SPACING);
                y = GRID_OFFSET_Y + ship->row * (GRID_CELL_SIZE + GRID_SPACING);
            } else {
                x = (int)ship->dock_pos.x;
                y = (int)ship->dock_pos.y;
            }
            
            if (ship->direction == 'H') {
                w = ship->length * (GRID_CELL_SIZE + GRID_SPACING) - GRID_SPACING;
                h = GRID_CELL_SIZE;
            } else {
                w = GRID_CELL_SIZE;
                h = ship->length * (GRID_CELL_SIZE + GRID_SPACING) - GRID_SPACING;
            }
            
            if (mx >= x && mx <= x + w && my >= y && my <= y + h) {
                dragged_ship_idx = i;
                ship->is_dragging = true;
                ship->drag_offset = (Vector2){ (float)(mx - x), (float)(my - y) };
                ship->is_placed = false; // Lift it up
                break;
            }
        }
    }
    
    // Handle dragging update/end
    if (dragged_ship_idx != -1) {
        DraggableShip *ship = &ui.ships[dragged_ship_idx];
        
        if (IsKeyPressed(KEY_R)) {
            ship->direction = (ship->direction == 'H') ? 'V' : 'H';
        }
        
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            ship->is_dragging = false;
            dragged_ship_idx = -1;
            
            // Check if dropped on grid
            int mx = GetMouseX();
            int my = GetMouseY();
            int row, col;
            
            // Use top-left of ship for snapping
            int ship_x = mx - (int)ship->drag_offset.x + (GRID_CELL_SIZE/2);
            int ship_y = my - (int)ship->drag_offset.y + (GRID_CELL_SIZE/2);
            
            if (screen_to_grid(ship_x, ship_y, GRID_OFFSET_X, &row, &col)) {
                // Validate bounds
                int end_row = row;
                int end_col = col;
                if (ship->direction == 'H') end_col += ship->length - 1;
                else end_row += ship->length - 1;
                
                if (end_row < GRID_ROWS && end_col < GRID_COLS) {
                    // Check overlap
                    int overlap = 0;
                    for (int j = 0; j < 5; j++) {
                        if (j == (ship - ui.ships)) continue; // Skip self
                        if (!ui.ships[j].is_placed) continue;
                        
                        int j_end_r = ui.ships[j].row;
                        int j_end_c = ui.ships[j].col;
                        if (ui.ships[j].direction == 'H') j_end_c += ui.ships[j].length - 1;
                        else j_end_r += ui.ships[j].length - 1;
                        
                        // Check intersection
                        int r_min = row, r_max = end_row;
                        int c_min = col, c_max = end_col;
                        int jr_min = ui.ships[j].row, jr_max = j_end_r;
                        int jc_min = ui.ships[j].col, jc_max = j_end_c;
                        
                        if (r_min <= jr_max && r_max >= jr_min && c_min <= jc_max && c_max >= jc_min) {
                            overlap = 1;
                            break;
                        }
                    }
                    
                    if (!overlap) {
                        ship->row = row;
                        ship->col = col;
                        ship->is_placed = true;
                    }
                }
            }
        }
    }
}

void handle_gameplay(void) {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        int row, col;
        // During gameplay, the grid is closer (offset 100)
        int right_grid_x = GRID_OFFSET_X + (GRID_COLS * (GRID_CELL_SIZE + GRID_SPACING)) + 100;
        
        if (screen_to_grid(GetMouseX(), GetMouseY(), right_grid_x, &row, &col)) {
            if (current_turn == ui.my_id) {
                client_send_fire(row, col);
                add_message("Fire! Waiting for result...");
            } else {
                add_message("Not your turn!");
            }
        }
    }
}

void handle_game_over(void) {
    if (IsKeyPressed(KEY_Y)) {
        client_send_play_again(1);
        add_message("Waiting for opponent to accept rematch...");
    }
    
    if (IsKeyPressed(KEY_N)) {
        client_send_play_again(0);
        add_message("Thanks for playing!");
    }
}
