#ifndef GAME_H
#define GAME_H

#include <stddef.h>

/* Default board size: 7 rows x 9 columns (columns displayed as letters A..I) */
#define GRID_ROWS 7
#define GRID_COLS 9

typedef struct Grid {
    void *cells;
    size_t elem_size;
    int rows;
    int cols;
} Grid;

/* Create/destroy a generic grid */
Grid *grid_create(int rows, int cols, size_t elem_size);
void grid_destroy(Grid *g);

/* Set/get using pointer arithmetic */
void grid_set(Grid *g, int r, int c, const void *elem);
void grid_get(Grid *g, int r, int c, void *out_elem);

/* Cell states for the game grid */
/* Cell values: 0=empty, 1-5=ship IDs, 'H'=hit, 'M'=miss to avoid conflicts */
enum Cell { CELL_EMPTY = 0, CELL_SHIP = 1, CELL_HIT = 'H', CELL_MISS = 'M' };

typedef struct Ship { int r, c, len; char dir; int id; } Ship;

/* Game logic functions */
int place_ship(Grid *g, Ship s);         /* Returns 1 if placed, 0 if invalid */
int fire_at(Grid *g, int r, int c);     /* Returns 1 if hit, 0 if miss, -1 if already fired */
int grid_has_ships(Grid *g);            /* Returns 1 if any ships remain */

#endif /* GAME_H */
