#include "app.h"
#include "fb.h"
#include "font.h"
#include "gomoku.h"

#define BOARD_X     11      /* pixel position of the top-left intersection */
#define BOARD_Y     11
#define CELL        11
#define STONE_R     5
#define PANEL_X     176
#define PANEL_CX    (PANEL_X + (FB_W - PANEL_X) / 2)

enum { SCR_MENU, SCR_GAME, SCR_PAUSE };
enum { MODE_EASY, MODE_NORMAL, MODE_HARD, MODE_TWO_PLAYER, NMODES };
enum { PAUSE_RESUME, PAUSE_UNDO, PAUSE_RESTART, PAUSE_HOME, NPAUSE };

#define MENU_REPLAY NMODES          /* replay the boot animation */
#define MENU_POWER  (NMODES + 1)    /* last main-menu entry */
#define NMENU       (NMODES + 2)

/* main menu is a 2-column grid */
static const char *const mode_menu[NMENU] = { "人机 · 简单", "人机 · 普通", "人机 · 困难", "双人对战", "从头开始", "关机" };
static const char *const mode_short[NMODES] = { "人机 · 简单", "人机 · 普通", "人机 · 困难", "双人对战" };
static const char *const pause_menu[NPAUSE] = { "继续游戏", "悔棋", "重新开始", "返回主菜单" };

static struct gm_game game;
static int screen, menu_sel, mode, pause_sel;
static int poweroff, replay;
static int cur_x, cur_y;
static int ai_pending;
static int full_refresh;
static uint32_t seed_counter;

static int vs_ai(void) { return mode != MODE_TWO_PLAYER; }

static void new_game(void)
{
    gm_reset(&game, ++seed_counter * 2654435761u);
    cur_x = cur_y = GM_N / 2;
    ai_pending = 0;
    screen = SCR_GAME;
    full_refresh = 1;
}

void app_init(uint32_t seed)
{
    seed_counter = seed;
    screen = SCR_MENU;
    menu_sel = MODE_NORMAL;
    full_refresh = 1;
}

int app_busy(void) { return ai_pending; }

int app_wants_poweroff(void) { return poweroff; }

int app_wants_replay(void)
{
    int r = replay;
    replay = 0;
    return r;
}

int app_wants_full_refresh(void)
{
    int f = full_refresh;
    full_refresh = 0;
    return f;
}

void app_step(void)
{
    int x, y;
    if (!ai_pending)
        return;
    ai_pending = 0;
    if (!game.winner && gm_ai_move(&game, (enum gm_level)mode, &x, &y) == 0)
        gm_play(&game, x, y);
}

int app_key(enum app_key k)
{
    if (screen == SCR_MENU) {
        switch (k) {
        case KEY_UP:    menu_sel = (menu_sel + NMENU - 2) % NMENU; return 1;
        case KEY_DOWN:  menu_sel = (menu_sel + 2) % NMENU; return 1;
        case KEY_LEFT:  menu_sel ^= 1; return 1;
        case KEY_RIGHT: menu_sel ^= 1; return 1;
        case KEY_OK:
            if (menu_sel == MENU_POWER) {
                poweroff = 1;
                return 0;
            }
            if (menu_sel == MENU_REPLAY) {
                replay = 1;
                full_refresh = 1;
                return 0;
            }
            mode = menu_sel;
            new_game();
            return 1;
        default:        return 0;
        }
    }

    if (screen == SCR_PAUSE) {
        switch (k) {
        case KEY_UP:    pause_sel = (pause_sel + NPAUSE - 1) % NPAUSE; return 1;
        case KEY_DOWN:  pause_sel = (pause_sel + 1) % NPAUSE; return 1;
        case KEY_MENU:  screen = SCR_GAME; return 1;
        case KEY_OK:
            screen = SCR_GAME;
            switch (pause_sel) {
            case PAUSE_UNDO:    app_key(KEY_UNDO); break;
            case PAUSE_RESTART: new_game(); break;
            case PAUSE_HOME:    screen = SCR_MENU; full_refresh = 1; break;
            }
            return 1;
        default:        return 0;
        }
    }

    if (ai_pending)
        return 0;

    switch (k) {
    case KEY_UP:    if (cur_y > 0) { cur_y--; return 1; } return 0;
    case KEY_DOWN:  if (cur_y < GM_N - 1) { cur_y++; return 1; } return 0;
    case KEY_LEFT:  if (cur_x > 0) { cur_x--; return 1; } return 0;
    case KEY_RIGHT: if (cur_x < GM_N - 1) { cur_x++; return 1; } return 0;

    case KEY_OK:
        if (game.winner) {
            new_game();
            return 1;
        }
        if (gm_play(&game, cur_x, cur_y) != 0)
            return 0;
        if (vs_ai() && !game.winner)
            ai_pending = 1;
        return 1;

    case KEY_UNDO:
        if (gm_undo(&game) != 0)
            return 0;
        /* against the computer, take back until it is the player's (black) turn again */
        while (vs_ai() && game.to_move != GM_BLACK && gm_undo(&game) == 0) {}
        if (game.moves) {
            cur_x = game.hist_x[game.moves - 1];
            cur_y = game.hist_y[game.moves - 1];
        }
        return 1;

    case KEY_MENU:
        screen = SCR_PAUSE;
        pause_sel = PAUSE_RESUME;
        return 1;
    }
    return 0;
}

/* ---- rendering ----------------------------------------------------------------------- */

/* "第 N 手" without pulling in stdio */
static void move_label(char *buf, int n)
{
    static const char pre[] = "第 ", post[] = " 手";
    char digits[4];
    int k = 0, i = 0;
    do { digits[k++] = (char)('0' + n % 10); n /= 10; } while (n && k < 4);
    for (const char *p = pre; *p; p++) buf[i++] = *p;
    while (k) buf[i++] = digits[--k];
    for (const char *p = post; *p; p++) buf[i++] = *p;
    buf[i] = 0;
}

static void text_center(uint8_t *fb, const struct font *f, int cx, int y, const char *s, int black)
{
    fb_text(fb, f, cx - fb_text_width(f, s) / 2, y, s, black);
}

static void draw_stone(uint8_t *fb, int px, int py, int color)
{
    if (color == GM_BLACK) {
        fb_disc(fb, px, py, STONE_R, 1);
    } else {
        fb_disc(fb, px, py, STONE_R, 0);
        fb_circle(fb, px, py, STONE_R, 1);
    }
}

static int on_win_line(int x, int y)
{
    if (game.winner <= 0)
        return 0;
    int dx = (game.win_x1 > game.win_x0) - (game.win_x1 < game.win_x0);
    int dy = (game.win_y1 > game.win_y0) - (game.win_y1 < game.win_y0);
    for (int cx = game.win_x0, cy = game.win_y0;; cx += dx, cy += dy) {
        if (cx == x && cy == y) return 1;
        if (cx == game.win_x1 && cy == game.win_y1) return 0;
    }
}

static void draw_board(uint8_t *fb)
{
    int end = (GM_N - 1) * CELL;
    for (int i = 0; i < GM_N; i++) {
        fb_hline(fb, BOARD_X, BOARD_X + end, BOARD_Y + i * CELL, 1);
        fb_vline(fb, BOARD_X + i * CELL, BOARD_Y, BOARD_Y + end, 1);
    }
    fb_rect(fb, BOARD_X - 2, BOARD_Y - 2, end + 5, end + 5, 1);

    static const uint8_t stars[5][2] = { {3, 3}, {11, 3}, {7, 7}, {3, 11}, {11, 11} };
    for (int i = 0; i < 5; i++)
        fb_disc(fb, BOARD_X + stars[i][0] * CELL, BOARD_Y + stars[i][1] * CELL, 1, 1);

    for (int y = 0; y < GM_N; y++)
        for (int x = 0; x < GM_N; x++) {
            int c = game.cell[y][x];
            if (c == GM_EMPTY)
                continue;
            int px = BOARD_X + x * CELL, py = BOARD_Y + y * CELL;
            draw_stone(fb, px, py, c);
            if (on_win_line(x, y)) {
                /* a small cross in the opposite colour marks the winning five */
                fb_line(fb, px - 2, py - 2, px + 2, py + 2, c != GM_BLACK);
                fb_line(fb, px - 2, py + 2, px + 2, py - 2, c != GM_BLACK);
            }
        }

    if (game.moves && game.winner <= 0) {
        int lx = game.hist_x[game.moves - 1], ly = game.hist_y[game.moves - 1];
        int c = game.cell[ly][lx];
        fb_fill(fb, BOARD_X + lx * CELL - 1, BOARD_Y + ly * CELL - 1, 3, 3, c != GM_BLACK);
    }

    if (!game.winner) {
        /* cursor: four corner brackets around the intersection */
        int px = BOARD_X + cur_x * CELL, py = BOARD_Y + cur_y * CELL, r = 7, a = 3;
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2) {
                int cx = px + sx * r, cy = py + sy * r;
                fb_hline(fb, cx, cx - sx * a, cy, 1);
                fb_vline(fb, cx, cy, cy - sy * a, 1);
            }
    }
}

static void draw_panel(uint8_t *fb)
{
    char buf[32];
    int me_black = game.to_move == GM_BLACK;

    fb_vline(fb, PANEL_X, 0, FB_H - 1, 1);
    text_center(fb, &font16, PANEL_CX, 6, "墨墨五子棋", 1);
    text_center(fb, &font12, PANEL_CX, 28, mode_short[mode], 1);
    fb_hline(fb, PANEL_X + 8, FB_W - 9, 46, 1);

    if (!game.winner) {
        draw_stone(fb, PANEL_X + 14, 63, game.to_move);
        const char *who;
        if (ai_pending) who = "电脑思考中";
        else if (vs_ai()) who = me_black ? "轮到你了" : "电脑走棋";
        else who = me_black ? "黑方走棋" : "白方走棋";
        fb_text(fb, &font12, PANEL_X + 24, 56, who, 1);
    }

    move_label(buf, game.moves);
    text_center(fb, &font12, PANEL_CX, 78, buf, 1);

    if (game.winner) {
        const char *msg;
        if (game.winner < 0) msg = "平局";
        else if (vs_ai()) msg = game.winner == GM_BLACK ? "你赢了！" : "电脑赢了";
        else msg = game.winner == GM_BLACK ? "黑方胜！" : "白方胜！";
        fb_fill(fb, PANEL_X + 6, 90, FB_W - PANEL_X - 12, 24, 1);
        text_center(fb, &font16, PANEL_CX, 94, msg, 0);
        text_center(fb, &font12, PANEL_CX, 117, "大圆键 再来一局", 1);
    }

    fb_hline(fb, PANEL_X + 8, FB_W - 9, 133, 1);
    text_center(fb, &font12, PANEL_CX, 137, "大圆键 落子", 1);
    text_center(fb, &font12, PANEL_CX, 150, "小圆键 悔棋", 1);
    text_center(fb, &font12, PANEL_CX, 163, "顶部键 菜单", 1);
}

static void draw_pause(uint8_t *fb)
{
    int x = 22, y = 28, w = 132, h = 122;
    fb_fill(fb, x, y, w, h, 0);
    fb_rect(fb, x, y, w, h, 1);
    fb_rect(fb, x + 2, y + 2, w - 4, h - 4, 1);
    text_center(fb, &font16, x + w / 2, y + 7, "暂停", 1);
    for (int i = 0; i < NPAUSE; i++) {
        int iy = y + 31 + i * 21;
        if (i == pause_sel) {
            fb_fill(fb, x + 10, iy - 3, w - 20, 21, 1);
            text_center(fb, &font16, x + w / 2, iy, pause_menu[i], 0);
        } else {
            text_center(fb, &font16, x + w / 2, iy, pause_menu[i], 1);
        }
    }
}

static void draw_menu(uint8_t *fb)
{
    text_center(fb, &font32, FB_W / 2, 8, "墨墨五子棋", 1);

    /* a few decorative stones beside the title */
    draw_stone(fb, 26, 28, GM_BLACK);
    draw_stone(fb, 38, 28, GM_WHITE);
    draw_stone(fb, 226, 28, GM_WHITE);
    draw_stone(fb, 238, 28, GM_BLACK);

    for (int i = 0; i < NMENU; i++) {
        int cx = i % 2 ? 192 : 72, y = 58 + (i / 2) * 30;
        if (i == menu_sel) {
            fb_fill(fb, cx - 58, y, 116, 24, 1);
            text_center(fb, &font16, cx, y, mode_menu[i], 0);
        } else {
            fb_rect(fb, cx - 58, y, 116, 24, 1);
            text_center(fb, &font16, cx, y, mode_menu[i], 1);
        }
    }
    text_center(fb, &font12, FB_W / 2, 159, "方向键 选择   大圆键 确认", 1);
}

void app_render(uint8_t *fb)
{
    fb_clear(fb);
    if (screen == SCR_MENU) {
        draw_menu(fb);
    } else {
        draw_board(fb);
        draw_panel(fb);
        if (screen == SCR_PAUSE)
            draw_pause(fb);
    }
}
