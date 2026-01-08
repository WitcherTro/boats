#include "game.h"
#include <stdlib.h>
#include <string.h>

Grid *grid_create(int rows, int cols, size_t elem_size) {
    Grid *g = malloc(sizeof(Grid));
    if (!g) return NULL;
    g->rows = rows;
    g->cols = cols;
    g->elem_size = elem_size;
    g->cells = calloc(rows * cols, elem_size);
    if (!g->cells) {
        free(g);
        return NULL;
    }
    return g;
}

void grid_destroy(Grid *g) {
    if (!g) return;
    free(g->cells);
    free(g);
}

void grid_set(Grid *g, int r, int c, const void *elem) {
    if (!g) return;
    if (r < 0 || r >= g->rows || c < 0 || c >= g->cols) return;
    char *base = (char *)g->cells;
    char *ptr = base + ((r * g->cols + c) * g->elem_size);
    memcpy(ptr, elem, g->elem_size);
}

void grid_get(Grid *g, int r, int c, void *out_elem) {
    if (!g) return;
    if (r < 0 || r >= g->rows || c < 0 || c >= g->cols) return;
    char *base = (char *)g->cells;
    char *ptr = base + ((r * g->cols + c) * g->elem_size);
    memcpy(out_elem, ptr, g->elem_size);
}

int place_ship(Grid *g, Ship s) {
    int dr = 0, dc = 1;
    if (s.dir == 'V' || s.dir == 'v') {
        dr = 1;
        dc = 0;
    }

    /* Check bounds and overlap */
    for (int i = 0; i < s.len; ++i) {
        int r = s.r + i * dr;
        int c = s.c + i * dc;
        if (r < 0 || r >= g->rows || c < 0 || c >= g->cols) return 0;
        unsigned char cur = CELL_EMPTY;
        grid_get(g, r, c, &cur);
        if (cur != CELL_EMPTY) return 0;  /* overlap */
    }

    /* Place ship with unique ID if provided, otherwise use CELL_SHIP */
    for (int i = 0; i < s.len; ++i) {
        int r = s.r + i * dr;
        int c = s.c + i * dc;
        unsigned char val = (s.id > 0 && s.id <= 5) ? (unsigned char)s.id : CELL_SHIP;
        grid_set(g, r, c, &val);
    }
    return 1;
}

int fire_at(Grid *g, int r, int c) {
    unsigned char val = CELL_EMPTY;
    grid_get(g, r, c, &val);

    /* If this cell was already targeted, return -1 to indicate invalid repeat attack */
    if (val == CELL_HIT || val == CELL_MISS) return -1;

    /* Check if val is a ship (either CELL_SHIP or ship IDs 1-5) */
    if (val == CELL_SHIP || (val >= 1 && val <= 5)) {
        unsigned char hit = CELL_HIT;
        grid_set(g, r, c, &hit);
        return 1;
    } else {
        unsigned char miss = CELL_MISS;
        grid_set(g, r, c, &miss);
        return 0;
    }
}

int grid_has_ships(Grid *g) {
    for (int r = 0; r < g->rows; ++r) {
        for (int c = 0; c < g->cols; ++c) {
            unsigned char val = CELL_EMPTY;
            grid_get(g, r, c, &val);
            /* Check for CELL_SHIP or ship IDs 1-5 */
            if (val == CELL_SHIP || (val >= 1 && val <= 5)) return 1;
        }
    }
    return 0;
}
