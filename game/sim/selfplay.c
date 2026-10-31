/* Host test: AI levels play each other; checks rules, undo, and timing. */
#include <stdio.h>
#include <time.h>
#include "../src/gomoku.h"

static int play(enum gm_level black, enum gm_level white, uint32_t seed, double *max_ms)
{
    struct gm_game g;
    gm_reset(&g, seed);
    while (!g.winner) {
        int x, y;
        clock_t t0 = clock();
        if (gm_ai_move(&g, g.to_move == GM_BLACK ? black : white, &x, &y) != 0)
            break;
        double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
        if (ms > *max_ms) *max_ms = ms;
        if (gm_play(&g, x, y) != 0) {
            printf("illegal move %d,%d\n", x, y);
            return -2;
        }
    }
    /* undo everything and make sure the board is empty again */
    int winner = g.winner, n = g.moves;
    while (gm_undo(&g) == 0) {}
    for (int y = 0; y < GM_N; y++)
        for (int x = 0; x < GM_N; x++)
            if (g.cell[y][x]) { printf("undo left a stone\n"); return -2; }
    if (g.to_move != GM_BLACK) { printf("undo broke turn order (%d moves)\n", n); return -2; }
    return winner;
}

int main(void)
{
    static const char *name[] = { "easy", "normal", "hard" };
    for (int b = 0; b < 3; b++)
        for (int w = 0; w < 3; w++) {
            int bw = 0, ww = 0, dr = 0;
            double max_ms = 0;
            for (uint32_t s = 1; s <= 10; s++) {
                int r = play(b, w, s * 7919, &max_ms);
                if (r == GM_BLACK) bw++; else if (r == GM_WHITE) ww++; else if (r == -1) dr++; else return 1;
            }
            printf("black %-6s vs white %-6s: black %2d  white %2d  draw %d   slowest move %.1f ms\n",
                   name[b], name[w], bw, ww, dr, max_ms);
        }
    return 0;
}
