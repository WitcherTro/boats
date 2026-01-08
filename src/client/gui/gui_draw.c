#include "gui_draw.h"
#include <string.h>

/* Helper to draw text with better readability - triple render for thickness */
void DrawTextBold(const char *text, int x, int y, int fontSize, Color color) {
    /* Draw shadow/outline for depth */
    DrawText(text, x - 1, y, fontSize, (Color){30, 30, 30, 100});
    DrawText(text, x + 1, y, fontSize, (Color){30, 30, 30, 100});
    DrawText(text, x, y - 1, fontSize, (Color){30, 30, 30, 100});
    DrawText(text, x, y + 1, fontSize, (Color){30, 30, 30, 100});
    /* Main text */
    DrawText(text, x, y, fontSize, color);
}

static void draw_cell(int px, int py, char cell, int is_own) {
    Color color = COLOR_WATER;
    
    if (is_own) {
        if (cell == 'S' || (cell >= 1 && cell <= 5)) {
            color = COLOR_SHIP;
        } else if (cell == 'H') {
            color = COLOR_HIT;
        } else if (cell == 'M') {
            color = COLOR_MISS;
        }
    } else {
        if (cell == 'H') {
            color = COLOR_HIT;
        } else if (cell == 'M') {
            color = COLOR_MISS;
        } else if (cell >= 1 && cell <= 5) {
            color = GRAY; /* Revealed ship */
        }
    }
    
    DrawRectangle(px, py, GRID_CELL_SIZE, GRID_CELL_SIZE, color);
    DrawRectangleLines(px, py, GRID_CELL_SIZE, GRID_CELL_SIZE, COLOR_BORDER);
}

void draw_grid(int x, int y, char grid[GRID_ROWS][GRID_COLS], int is_own) {
    /* Column headers A-I */
    for (int c = 0; c < GRID_COLS; c++) {
        DrawTextBold(TextFormat("%c", 'A' + c), 
                x + c * (GRID_CELL_SIZE + GRID_SPACING) + 14, y - 35, 22, COLOR_TEXT);
    }
    
    /* Row numbers 1-7 */
    for (int r = 0; r < GRID_ROWS; r++) {
        DrawTextBold(TextFormat("%d", r + 1), x - 34, y + r * (GRID_CELL_SIZE + GRID_SPACING) + 8, 22, COLOR_TEXT);
        
        for (int c = 0; c < GRID_COLS; c++) {
            int px = x + c * (GRID_CELL_SIZE + GRID_SPACING);
            int py = y + r * (GRID_CELL_SIZE + GRID_SPACING);
            
            draw_cell(px, py, grid[r][c], is_own);
        }
    }
    
    /* Draw ship placement preview during placement phase */
    if (is_own && ui.state == STATE_SHIP_PLACEMENT && ui.selected_row >= 0) {
        int current_ship_size = ship_sizes[ui.ships_placed];
        int px = x + ui.selected_col * (GRID_CELL_SIZE + GRID_SPACING);
        int py = y + ui.selected_row * (GRID_CELL_SIZE + GRID_SPACING);
        
        if (ui.placing_direction == 'H') {
            DrawRectangle(px, py, current_ship_size * (GRID_CELL_SIZE + GRID_SPACING), GRID_CELL_SIZE, 
                         (Color){255, 255, 0, 100});
        } else {
            DrawRectangle(px, py, GRID_CELL_SIZE, current_ship_size * (GRID_CELL_SIZE + GRID_SPACING), 
                         (Color){255, 255, 0, 100});
        }
    }
}

void draw_ships(void) {
    if (ui.state != STATE_SHIP_PLACEMENT) return;

    for (int i = 0; i < 5; i++) {
        DraggableShip *ship = &ui.ships[i];
        int x, y;
        int w, h;
        
        if (ship->is_dragging) {
            x = GetMouseX() - (int)ship->drag_offset.x;
            y = GetMouseY() - (int)ship->drag_offset.y;
        } else if (ship->is_placed) {
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
        
        // Draw ship body
        DrawRectangle(x, y, w, h, COLOR_SHIP);
        DrawRectangleLines(x, y, w, h, BLACK);
        
        // Draw label
        DrawText(TextFormat("%d", ship->length), x + 5, y + 5, 20, WHITE);
    }
    
    // Draw "Confirm" button if all placed
    int placed_count = 0;
    for(int i=0; i<5; i++) if(ui.ships[i].is_placed) placed_count++;
    
    if (placed_count == 5) {
        int mx = GetMouseX();
        int my = GetMouseY();
        Color btnColor = GREEN;
        
        if (mx >= 600 && mx <= 800 && my >= 500 && my <= 550) {
            btnColor = (Color){ 0, 230, 0, 255 }; // Highlight on hover
        }
        
        DrawRectangle(600, 500, 200, 50, btnColor);
        DrawText("CONFIRM", 640, 515, 30, BLACK);
    } else {
        DrawText("Drag ships to grid", 600, 500, 20, GRAY);
        DrawText("Press R to rotate", 600, 530, 20, GRAY);
    }
}

void draw_messages(void) {
    int y = WINDOW_HEIGHT - 40;
    /* Draw newest messages at the bottom, older ones above */
    for (int i = msg_count - 1; i >= 0; i--) {
        if (messages[i].fade_time > 0) {
            float alpha = (messages[i].fade_time / 5.0f);
            if (alpha > 1.0f) alpha = 1.0f;
            
            Color msg_color = COLOR_TEXT;
            
            /* Highlight important messages */
            if (strstr(messages[i].text, "SUNK") != NULL) {
                msg_color = RED;
            } else if (strstr(messages[i].text, "WIN") != NULL) {
                msg_color = GREEN;
            } else if (strstr(messages[i].text, "LOSE") != NULL) {
                msg_color = RED;
            }

            msg_color.a = (unsigned char)(255 * alpha);
            DrawTextBold(messages[i].text, 60, y, 20, msg_color);
            y -= 28;
        }
    }
}

void draw_ui_info(void) {
    int y = 20;
    DrawTextBold("Battleship", 60, y, 40, COLOR_TEXT);
    
    if (ui.player_name[0]) {
        DrawTextBold(TextFormat("You: %s", ui.player_name), 300, y + 5, 20, COLOR_TEXT);
    }
    if (ui.opponent_name[0]) {
        DrawTextBold(TextFormat("Opp: %s", ui.opponent_name), 300, y + 30, 20, COLOR_TEXT);
    }
}
