/*
 * Gomoku rules and AI (free-style: five or more in a row wins).
 *
 * The AI scores every 5-cell window ("tuple") on the board. For choosing a move it uses the
 * classic tuple table, which weighs both building our own lines and blocking the opponent's.
 * The hard level adds a negamax alpha-beta search on top, using the same table to order moves.
 */
#include <string.h>
#include "gomoku.h"

#define NTUPLES     572
#define MAX_CAND    64
#define WIN_SCORE   100000000

static uint8_t  tup_cell[NTUPLES][5];   /* cell index = y * GM_N + x */
static uint16_t cell_tups[GM_CELLS][20];
static uint8_t  cell_ntups[GM_CELLS];
static int      tables_ready;

static const int dirs[4][2] = { {1, 0}, {0, 1}, {1, 1}, {1, -1} };

static void build_lines(void);

static void build_tables(void)
{
    int n = 0;
    for (int d = 0; d < 4; d++) {
        for (int y = 0; y < GM_N; y++) {
            for (int x = 0; x < GM_N; x++) {
                int ex = x + dirs[d][0] * 4, ey = y + dirs[d][1] * 4;
                if (ex < 0 || ex >= GM_N || ey < 0 || ey >= GM_N)
                    continue;
                for (int k = 0; k < 5; k++) {
                    int c = (y + dirs[d][1] * k) * GM_N + (x + dirs[d][0] * k);
                    tup_cell[n][k] = (uint8_t)c;
                    cell_tups[c][cell_ntups[c]++] = (uint16_t)n;
                }
                n++;
            }
        }
    }
    tables_ready = 1;
}

static uint32_t rnd(struct gm_game *g)
{
    uint32_t x = g->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    g->rng = x ? x : 0x9e3779b9u;
    return g->rng;
}

void gm_reset(struct gm_game *g, uint32_t seed)
{
    if (!tables_ready) {
        build_tables();
        build_lines();
    }
    memset(g, 0, sizeof(*g));
    g->to_move = GM_BLACK;
    g->rng = seed ? seed : 0x12345678u;
}

static int count_dir(const struct gm_game *g, int x, int y, int dx, int dy, int color)
{
    int n = 0;
    for (x += dx, y += dy; x >= 0 && x < GM_N && y >= 0 && y < GM_N && g->cell[y][x] == color; x += dx, y += dy)
        n++;
    return n;
}

int gm_play(struct gm_game *g, int x, int y)
{
    if (g->winner || x < 0 || x >= GM_N || y < 0 || y >= GM_N || g->cell[y][x] != GM_EMPTY)
        return -1;

    int color = g->to_move;
    g->cell[y][x] = (uint8_t)color;
    g->hist_x[g->moves] = (uint8_t)x;
    g->hist_y[g->moves] = (uint8_t)y;
    g->moves++;

    for (int d = 0; d < 4; d++) {
        int a = count_dir(g, x, y, dirs[d][0], dirs[d][1], color);
        int b = count_dir(g, x, y, -dirs[d][0], -dirs[d][1], color);
        if (a + b + 1 >= 5) {
            g->winner = color;
            g->win_x0 = x - dirs[d][0] * b;  g->win_y0 = y - dirs[d][1] * b;
            g->win_x1 = x + dirs[d][0] * a;  g->win_y1 = y + dirs[d][1] * a;
            break;
        }
    }
    if (!g->winner && g->moves == GM_CELLS)
        g->winner = -1;

    g->to_move = color == GM_BLACK ? GM_WHITE : GM_BLACK;
    return 0;
}

int gm_undo(struct gm_game *g)
{
    if (g->moves == 0)
        return -1;
    g->moves--;
    g->cell[g->hist_y[g->moves]][g->hist_x[g->moves]] = GM_EMPTY;
    g->to_move = g->to_move == GM_BLACK ? GM_WHITE : GM_BLACK;
    g->winner = GM_EMPTY;
    return 0;
}

/* ---- evaluation ---------------------------------------------------------------------- */

static void tuple_counts(const uint8_t *b, int t, int me, int *own, int *opp)
{
    int o = 0, p = 0;
    for (int k = 0; k < 5; k++) {
        int v = b[tup_cell[t][k]];
        if (v == me) o++;
        else if (v != GM_EMPTY) p++;
    }
    *own = o;
    *opp = p;
}

/* Value of playing into a tuple that currently holds own/opp stones (classic table). */
static int32_t tuple_move_value(int own, int opp)
{
    static const int32_t own_v[5] = { 7, 35, 800, 15000, 800000 };
    static const int32_t opp_v[5] = { 7, 15, 400, 1800, 100000 };
    if (own && opp) return 0;
    return own ? own_v[own] : opp_v[opp];
}

static int32_t cell_value(const uint8_t *b, int c, int me)
{
    int32_t s = 0;
    for (int i = 0; i < cell_ntups[c]; i++) {
        int own, opp;
        tuple_counts(b, cell_tups[c][i], me, &own, &opp);
        s += tuple_move_value(own, opp);
    }
    return s;
}

/* ---- shape-based static evaluation --------------------------------------------------- */

enum { SH_FIVE, SH_OPEN4, SH_FOUR, SH_OPEN3, SH_THREE, SH_OPEN2, SH_TWO, NSHAPES };

/* x = own stone, o = opponent stone or board edge, . = empty */
static const struct { const char *pat; uint8_t shape; } shape_pats[] = {
    { "xxxxx",  SH_FIVE },
    { ".xxxx.", SH_OPEN4 },
    { "xxxx.",  SH_FOUR }, { ".xxxx", SH_FOUR }, { "x.xxx", SH_FOUR }, { "xx.xx", SH_FOUR }, { "xxx.x", SH_FOUR },
    { ".xxx..", SH_OPEN3 }, { "..xxx.", SH_OPEN3 }, { ".xx.x.", SH_OPEN3 }, { ".x.xx.", SH_OPEN3 },
    { "oxxx..", SH_THREE }, { "..xxxo", SH_THREE }, { "oxx.x.", SH_THREE }, { ".x.xxo", SH_THREE },
    { "ox.xx.", SH_THREE }, { ".xx.xo", SH_THREE }, { "x..xx", SH_THREE }, { "xx..x", SH_THREE },
    { "x.x.x",  SH_THREE }, { "o.xxx.o", SH_THREE },
    { "..xx..", SH_OPEN2 }, { ".x.x.", SH_OPEN2 }, { ".x..x.", SH_OPEN2 },
    { "oxx...", SH_TWO }, { "...xxo", SH_TWO }, { "ox.x..", SH_TWO }, { "..x.xo", SH_TWO },
};

#define NLINES 72
static uint8_t line_len[NLINES];
static uint8_t line_cell[NLINES][GM_N];

static void build_lines(void)
{
    int n = 0;
    for (int i = 0; i < GM_N; i++) {
        line_len[n] = GM_N;
        for (int k = 0; k < GM_N; k++) line_cell[n][k] = (uint8_t)(i * GM_N + k);
        n++;
        line_len[n] = GM_N;
        for (int k = 0; k < GM_N; k++) line_cell[n][k] = (uint8_t)(k * GM_N + i);
        n++;
    }
    for (int s = 4; s <= 2 * (GM_N - 1) - 4; s++) {    /* anti-diagonals x + y = s */
        int len = 0;
        for (int x = 0; x < GM_N; x++) {
            int y = s - x;
            if (y >= 0 && y < GM_N) line_cell[n][len++] = (uint8_t)(y * GM_N + x);
        }
        line_len[n++] = (uint8_t)len;
    }
    for (int d = -(GM_N - 5); d <= GM_N - 5; d++) {    /* diagonals x - y = d */
        int len = 0;
        for (int y = 0; y < GM_N; y++) {
            int x = y + d;
            if (x >= 0 && x < GM_N) line_cell[n][len++] = (uint8_t)(y * GM_N + x);
        }
        line_len[n++] = (uint8_t)len;
    }
}

static void count_shapes(const uint8_t *b, int me, int cnt[NSHAPES])
{
    char buf[GM_N + 2];
    for (int i = 0; i < NSHAPES; i++) cnt[i] = 0;

    for (int l = 0; l < NLINES; l++) {
        int len = line_len[l] + 2;
        buf[0] = buf[len - 1] = 'o';
        int stones = 0;
        for (int k = 0; k < line_len[l]; k++) {
            int v = b[line_cell[l][k]];
            buf[k + 1] = v == GM_EMPTY ? '.' : v == me ? (stones++, 'x') : 'o';
        }
        if (stones < 2)
            continue;
        for (int p = 0; p < (int)(sizeof(shape_pats) / sizeof(shape_pats[0])); p++) {
            const char *pat = shape_pats[p].pat;
            int pl = (int)strlen(pat);
            for (int s = 0; s + pl <= len; s++) {
                int k = 0;
                while (k < pl && pat[k] == buf[s + k]) k++;
                if (k == pl) cnt[shape_pats[p].shape]++;
            }
        }
    }
}

/* Static evaluation from the point of view of `me`, who is about to move. */
static int32_t evaluate(const uint8_t *b, int me)
{
    static const int32_t w[NSHAPES] = { 0, 0, 2500, 3000, 300, 250, 30 };
    int opp = me == GM_BLACK ? GM_WHITE : GM_BLACK;
    int m[NSHAPES], o[NSHAPES];
    count_shapes(b, me, m);
    count_shapes(b, opp, o);

    if (m[SH_FIVE]) return WIN_SCORE / 2;
    if (o[SH_FIVE]) return -WIN_SCORE / 2;
    if (m[SH_OPEN4] || m[SH_FOUR]) return WIN_SCORE / 2;             /* we complete five now */
    if (o[SH_OPEN4] || o[SH_FOUR] >= 2) return -WIN_SCORE / 2;       /* two ways to five: can't block both */
    if (!o[SH_FOUR] && m[SH_OPEN3]) return WIN_SCORE / 4;            /* we make an open four next */
    if (o[SH_OPEN3] >= 2 || (o[SH_OPEN3] && o[SH_FOUR])) return -WIN_SCORE / 4;

    int32_t mine = 0, theirs = 0;
    for (int i = 0; i < NSHAPES; i++) {
        mine += w[i] * m[i];
        theirs += w[i] * o[i];
    }
    return mine + mine / 8 - theirs;
}

static int makes_five(const uint8_t *b, int c, int color)
{
    int x = c % GM_N, y = c / GM_N;
    for (int d = 0; d < 4; d++) {
        int n = 1;
        for (int s = -1; s <= 1; s += 2) {
            int cx = x + dirs[d][0] * s, cy = y + dirs[d][1] * s;
            while (cx >= 0 && cx < GM_N && cy >= 0 && cy < GM_N && b[cy * GM_N + cx] == color) {
                n++;
                cx += dirs[d][0] * s;
                cy += dirs[d][1] * s;
            }
        }
        if (n >= 5) return 1;
    }
    return 0;
}

/* Empty cells within distance 2 of a stone, best first by the tuple table. */
static int gen_candidates(const uint8_t *b, int me, int *out, int32_t *score, int max)
{
    int n = 0;
    for (int c = 0; c < GM_CELLS; c++) {
        if (b[c] != GM_EMPTY)
            continue;
        int x = c % GM_N, y = c / GM_N, near = 0;
        for (int dy = -2; dy <= 2 && !near; dy++)
            for (int dx = -2; dx <= 2 && !near; dx++) {
                int nx = x + dx, ny = y + dy;
                if (nx >= 0 && nx < GM_N && ny >= 0 && ny < GM_N && b[ny * GM_N + nx] != GM_EMPTY)
                    near = 1;
            }
        if (!near)
            continue;

        int32_t s = cell_value(b, c, me);
        /* insertion into the sorted top-`max` list */
        int i;
        if (n < max) {
            i = n++;
        } else {
            if (s <= score[max - 1])
                continue;
            i = max - 1;
        }
        while (i > 0 && score[i - 1] < s) {
            out[i] = out[i - 1];
            score[i] = score[i - 1];
            i--;
        }
        out[i] = c;
        score[i] = s;
    }
    return n;
}

static int32_t negamax(uint8_t *b, int me, int depth, int width, int32_t alpha, int32_t beta, int ply)
{
    int cand[MAX_CAND];
    int32_t cs[MAX_CAND];
    int opp = me == GM_BLACK ? GM_WHITE : GM_BLACK;

    int n = gen_candidates(b, me, cand, cs, width);
    if (n == 0)
        return 0;

    for (int i = 0; i < n; i++)
        if (makes_five(b, cand[i], me))
            return WIN_SCORE - ply;

    if (depth == 0)
        return evaluate(b, me);

    int32_t best = -WIN_SCORE;
    for (int i = 0; i < n; i++) {
        b[cand[i]] = (uint8_t)me;
        int32_t v = -negamax(b, opp, depth - 1, width, -beta, -alpha, ply + 1);
        b[cand[i]] = GM_EMPTY;
        if (v > best) best = v;
        if (v > alpha) alpha = v;
        if (alpha >= beta) break;
    }
    return best;
}

int gm_ai_move(struct gm_game *g, enum gm_level level, int *x, int *y)
{
    uint8_t *b = &g->cell[0][0];
    int me = g->to_move, opp = me == GM_BLACK ? GM_WHITE : GM_BLACK;
    int cand[MAX_CAND];
    int32_t cs[MAX_CAND];

    if (g->moves >= GM_CELLS)
        return -1;
    if (g->moves == 0) {
        *x = GM_N / 2;
        *y = GM_N / 2;
        return 0;
    }

    int n = gen_candidates(b, me, cand, cs, level == GM_HARD ? 12 : 6);
    int pick = cand[0];

    /* always take a win, and block the opponent's five, at every level */
    for (int i = 0; i < n; i++)
        if (makes_five(b, cand[i], me)) { pick = cand[i]; goto done; }
    for (int i = 0; i < n; i++)
        if (makes_five(b, cand[i], opp)) { pick = cand[i]; goto done; }

    if (level == GM_EASY) {
        /* pick among the top few, favouring the best, and sometimes overlook a threat */
        uint32_t r = rnd(g) % 16;
        int idx = r < 8 ? 0 : r < 12 ? 1 : r < 14 ? 2 : 3;
        pick = cand[idx < n ? idx : n - 1];
    } else if (level == GM_NORMAL) {
        int ties = 1;
        while (ties < n && cs[ties] == cs[0]) ties++;
        pick = cand[rnd(g) % ties];
    } else {
        int32_t best = -WIN_SCORE - 1, alpha = -WIN_SCORE, beta = WIN_SCORE;
        for (int i = 0; i < n; i++) {
            b[cand[i]] = (uint8_t)me;
            int32_t v = -negamax(b, opp, 3, 8, -beta, -alpha, 1);
            b[cand[i]] = GM_EMPTY;
            if (v > best) { best = v; pick = cand[i]; }
            if (v > alpha) alpha = v;
        }
    }

done:
    *x = pick % GM_N;
    *y = pick / GM_N;
    return 0;
}
