#include "gui_state.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

/* Global State Definitions */
UIState ui = {
    .state = STATE_CONNECTING,
    .my_id = -1,
    .opponent_id = -1,
    .current_turn = -1,
    .winner_id = -1,
    .name_input_len = 0,
    .placing_direction = 'H',
    .selected_row = -1,
    .selected_col = -1,
    .ships_placed = 0,
    .placement_confirmed = 0,
    .auto_placing = 0,
    .auto_place_ship_idx = 0,
    .auto_place_attempts = 0,
    .auto_place_last_placed = 0,
    .server_ip = "127.0.0.1",
    .server_port_str = "12345",
    .ip_input_len = 9,
    .port_input_len = 5,
    .active_input_field = 0
};

Message messages[MAX_MESSAGES];
int msg_count = 0;
int ship_sizes[5] = {5, 4, 3, 3, 2};

void add_message(const char *fmt, ...) {
    va_list args;
    if (msg_count >= MAX_MESSAGES) {
        for (int i = 0; i < MAX_MESSAGES - 1; i++) {
            messages[i] = messages[i + 1];
        }
        msg_count = MAX_MESSAGES - 1;
    }
    
    va_start(args, fmt);
    vsnprintf(messages[msg_count].text, sizeof(messages[msg_count].text), fmt, args);
    va_end(args);
    
    messages[msg_count].fade_time = 8.0f;
    msg_count++;
}

void init_ships(void) {
    int start_y = GRID_OFFSET_Y;
    for (int i = 0; i < 5; i++) {
        ui.ships[i].length = ship_sizes[i];
        ui.ships[i].row = -1;
        ui.ships[i].col = -1;
        ui.ships[i].direction = 'H';
        ui.ships[i].is_placed = false;
        ui.ships[i].is_dragging = false;
        ui.ships[i].dock_pos = (Vector2){ 600, (float)(start_y + i * 60) };
        ui.ships[i].drag_offset = (Vector2){ 0, 0 };
    }
}

int screen_to_grid(int mouse_x, int mouse_y, int grid_x, int *out_row, int *out_col) {
    int rel_x = mouse_x - grid_x;
    int rel_y = mouse_y - GRID_OFFSET_Y;
    
    if (rel_x < 0 || rel_y < 0) return 0;
    
    int col = rel_x / (GRID_CELL_SIZE + GRID_SPACING);
    int row = rel_y / (GRID_CELL_SIZE + GRID_SPACING);
    
    if (col >= GRID_COLS || row >= GRID_ROWS) return 0;
    
    *out_row = row;
    *out_col = col;
    return 1;
}
