#ifndef CLIENT_UI_H
#define CLIENT_UI_H

/*
 * client_ui.h - Client UI/display utilities for grid visualization
 */

/* Display both grids side-by-side with colored pieces and legend */
void show_grids(void);

/* Initialize grids to empty state and reset remaining ship counts */
void init_grids(void);

/* Print UTF-8 string, using WriteConsoleW on Windows when UTF-8 console is available */
void print_utf8(const char *str);

#endif /* CLIENT_UI_H */
