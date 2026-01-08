#ifndef GUI_DRAW_H
#define GUI_DRAW_H

#include "gui_state.h"

void DrawTextBold(const char *text, int x, int y, int fontSize, Color color);
void draw_grid(int x, int y, char grid[GRID_ROWS][GRID_COLS], int is_own);
void draw_ships(void);
void draw_ui_info(void);
void draw_messages(void);

#endif /* GUI_DRAW_H */
