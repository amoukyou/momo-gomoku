#include <string.h>
#include "fb.h"

void fb_clear(uint8_t *fb)
{
    memset(fb, 0, FB_BYTES);
}

void fb_pixel(uint8_t *fb, int x, int y, int black)
{
    if ((unsigned)x >= FB_W || (unsigned)y >= FB_H)
        return;
    uint8_t *p = &fb[y * FB_STRIDE + (x >> 3)];
    uint8_t m = (uint8_t)(0x80 >> (x & 7));
    if (black) *p |= m; else *p &= (uint8_t)~m;
}

void fb_hline(uint8_t *fb, int x0, int x1, int y, int black)
{
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    for (int x = x0; x <= x1; x++)
        fb_pixel(fb, x, y, black);
}

void fb_vline(uint8_t *fb, int x, int y0, int y1, int black)
{
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    for (int y = y0; y <= y1; y++)
        fb_pixel(fb, x, y, black);
}

void fb_line(uint8_t *fb, int x0, int y0, int x1, int y1, int black)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        fb_pixel(fb, x0, y0, black);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void fb_rect(uint8_t *fb, int x, int y, int w, int h, int black)
{
    fb_hline(fb, x, x + w - 1, y, black);
    fb_hline(fb, x, x + w - 1, y + h - 1, black);
    fb_vline(fb, x, y, y + h - 1, black);
    fb_vline(fb, x + w - 1, y, y + h - 1, black);
}

void fb_fill(uint8_t *fb, int x, int y, int w, int h, int black)
{
    for (int j = y; j < y + h; j++)
        fb_hline(fb, x, x + w - 1, j, black);
}

void fb_circle(uint8_t *fb, int cx, int cy, int r, int black)
{
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        fb_pixel(fb, cx + x, cy + y, black); fb_pixel(fb, cx - x, cy + y, black);
        fb_pixel(fb, cx + x, cy - y, black); fb_pixel(fb, cx - x, cy - y, black);
        fb_pixel(fb, cx + y, cy + x, black); fb_pixel(fb, cx - y, cy + x, black);
        fb_pixel(fb, cx + y, cy - x, black); fb_pixel(fb, cx - y, cy - x, black);
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}

void fb_disc(uint8_t *fb, int cx, int cy, int r, int black)
{
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r + r)
                fb_pixel(fb, cx + x, cy + y, black);
}

/* ---- text ---------------------------------------------------------------------------- */

static uint32_t utf8_next(const char **ps)
{
    const uint8_t *s = (const uint8_t *)*ps;
    uint32_t c = *s++;
    if (c >= 0xf0)      { c = (c & 0x07) << 18; c |= (*s++ & 0x3f) << 12; c |= (*s++ & 0x3f) << 6; c |= *s++ & 0x3f; }
    else if (c >= 0xe0) { c = (c & 0x0f) << 12; c |= (*s++ & 0x3f) << 6; c |= *s++ & 0x3f; }
    else if (c >= 0xc0) { c = (c & 0x1f) << 6; c |= *s++ & 0x3f; }
    *ps = (const char *)s;
    return c;
}

static const struct glyph *find_glyph(const struct font *f, uint32_t cp)
{
    int lo = 0, hi = f->count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (f->glyphs[mid].cp == cp) return &f->glyphs[mid];
        if (f->glyphs[mid].cp < cp) lo = mid + 1; else hi = mid - 1;
    }
    return 0;
}

int fb_text(uint8_t *fb, const struct font *f, int x, int y, const char *s, int black)
{
    while (*s) {
        uint32_t cp = utf8_next(&s);
        const struct glyph *g = find_glyph(f, cp);
        if (!g) g = find_glyph(f, '?');
        if (!g) continue;
        int stride = (g->w + 7) / 8;
        for (int j = 0; j < g->h; j++)
            for (int i = 0; i < g->w; i++)
                if (g->bits[j * stride + i / 8] & (0x80 >> (i & 7)))
                    fb_pixel(fb, x + g->dx + i, y + g->dy + j, black);
        x += g->adv;
    }
    return x;
}

int fb_text_width(const struct font *f, const char *s)
{
    int w = 0;
    while (*s) {
        const struct glyph *g = find_glyph(f, utf8_next(&s));
        if (g) w += g->adv;
    }
    return w;
}

/* ---- frame diff ---------------------------------------------------------------------- */

int fb_diff(const uint8_t *a, const uint8_t *b, struct fb_rect *out, int max)
{
    int n = 0, y = 0;
    while (y < FB_H && n < max) {
        /* find the next run of changed rows, then the byte span changed within it */
        int x0 = FB_STRIDE, x1 = -1;
        for (int i = 0; i < FB_STRIDE; i++)
            if (a[y * FB_STRIDE + i] != b[y * FB_STRIDE + i]) { if (i < x0) x0 = i; x1 = i; }
        if (x1 < 0) { y++; continue; }

        int y0 = y;
        int gap = 0;
        for (y++; y < FB_H; y++) {
            int changed = 0;
            for (int i = 0; i < FB_STRIDE; i++)
                if (a[y * FB_STRIDE + i] != b[y * FB_STRIDE + i]) {
                    changed = 1;
                    if (i < x0) x0 = i;
                    if (i > x1) x1 = i;
                }
            if (changed) gap = 0;
            else if (++gap > 8) break;      /* runs separated by more than 8 clean rows are split */
        }
        int y1 = y - gap - 1;
        out[n].x = (int16_t)(x0 * 8);
        out[n].w = (int16_t)((x1 - x0 + 1) * 8);
        out[n].y = (int16_t)y0;
        out[n].h = (int16_t)(y1 - y0 + 1);
        n++;
    }
    if (y < FB_H && n == max) {
        /* too many regions: fall back to one rectangle from the last one to the bottom */
        out[max - 1].x = 0;
        out[max - 1].w = FB_W;
        out[max - 1].h = (int16_t)(FB_H - out[max - 1].y);
    }
    return n;
}
