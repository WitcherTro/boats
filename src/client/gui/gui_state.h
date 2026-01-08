#ifndef GUI_STATE_H
#define GUI_STATE_H

#include "raylib.h"
#include <stdio.h>

/* ==================== Configuration ==================== */

#define WINDOW_WIDTH 1400
#define WINDOW_HEIGHT 800
#define FPS 60

#define GRID_CELL_SIZE 50
#define GRID_OFFSET_X 60
#define GRID_OFFSET_Y 160
#define GRID_SPACING 1
#define GRID_ROWS 7
#define GRID_COLS 9

#define COLOR_EMPTY    ((Color){200, 200, 200, 255})
#define COLOR_SHIP     ((Color){0, 100, 200, 255})
#define COLOR_HIT      ((Color){200, 0, 0, 255})
#define COLOR_MISS     ((Color){100, 100, 100, 255})
#define COLOR_WATER    ((Color){100, 150, 200, 255})
#define COLOR_SELECT   ((Color){255, 200, 0, 255})
#define COLOR_BG       ((Color){240, 240, 240, 255})
#define COLOR_TEXT     ((Color){50, 50, 50, 255})
#define COLOR_BORDER   ((Color){0, 0, 0, 255})

/* ==================== Game State ==================== */

typedef enum {
    STATE_CONNECT_INPUT,
    STATE_CONNECTING,
    STATE_NAME_INPUT,
    STATE_WAITING_PLACEMENT,
    STATE_SHIP_PLACEMENT,
    STATE_WAITING_OPPONENT,
    STATE_PLAYING,
    STATE_GAME_OVER
} GameFlowState;

typedef struct {
    int length;
    int row, col;      // Grid coordinates if placed, -1 if not
    char direction;    // 'H' or 'V'
    bool is_placed;
    bool is_dragging;
    Vector2 dock_pos;  // Home position
    Vector2 drag_offset;
} DraggableShip;

typedef struct {
    GameFlowState state;
    char player_name[64];
    char opponent_name[64];
    int name_input_len;
    int my_id;
    int opponent_id;
    int current_turn;
    int winner_id;
    int ships_placed;
    int opponent_ready;
    
    /* Ship placement */
    char placing_direction;
    int selected_row, selected_col;
    int placement_confirmed;
    
    /* Draggable ships */
    DraggableShip ships[5];
    
    /* Auto-placement state */
    int auto_placing;
    int auto_place_ship_idx;
    int auto_place_attempts;
    int auto_place_last_placed;
    
    /* Connection input */
    char server_ip[64];
    char server_port_str[16];
    int ip_input_len;
    int port_input_len;
    int active_input_field; // 0: IP, 1: Port
} UIState;

/* Message log for feedback */
#define MAX_MESSAGES 6
typedef struct {
    char text[256];
    float fade_time;
} Message;

/* Global State */
extern UIState ui;
extern Message messages[MAX_MESSAGES];
extern int msg_count;
extern int ship_sizes[5];

/* External Globals (from client_globals.c) */
extern char own_grid[GRID_ROWS][GRID_COLS];
extern char opp_grid[GRID_ROWS][GRID_COLS];
extern int my_id;
extern volatile int current_turn;
extern volatile int server_disconnected;
extern int placed_count;

/* Helper Functions */
void add_message(const char *fmt, ...);
void init_ships(void);
int screen_to_grid(int mouse_x, int mouse_y, int grid_x, int *out_row, int *out_col);

#endif /* GUI_STATE_H */
