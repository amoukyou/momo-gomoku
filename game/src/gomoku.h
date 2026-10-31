/* Gomoku rules and AI. Portable C99, no allocation. */
#ifndef GOMOKU_H
#define GOMOKU_H

#include <stdint.h>

#define GM_N        15
#define GM_CELLS    (GM_N * GM_N)

enum { GM_EMPTY = 0, GM_BLACK = 1, GM_WHITE = 2 };

enum gm_level { GM_EASY = 0, GM_NORMAL = 1, GM_HARD = 2 };

struct gm_game {
    uint8_t  cell[GM_N][GM_N];
    uint8_t  hist_x[GM_CELLS];
    uint8_t  hist_y[GM_CELLS];
    int      moves;          /* number of stones on the board */
    int      to_move;        /* GM_BLACK or GM_WHITE */
    int      winner;         /* GM_EMPTY while playing, -1 for a draw */
    int      win_x0, win_y0, win_x1, win_y1;   /* ends of the winning line */
    uint32_t rng;
};

void gm_reset(struct gm_game *g, uint32_t seed);

/* Place a stone for the side to move. Returns 0 on success, -1 if illegal. */
int  gm_play(struct gm_game *g, int x, int y);

/* Take back the last move. Returns 0 on success, -1 if there is nothing to undo. */
int  gm_undo(struct gm_game *g);

/* Choose a move for the side to move. Returns 0 and fills x/y, or -1 if the board is full. */
int  gm_ai_move(struct gm_game *g, enum gm_level level, int *x, int *y);

#endif
