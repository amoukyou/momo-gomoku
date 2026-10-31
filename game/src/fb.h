/* 1-bit framebuffer for the 264x176 e-ink panel. Bit set = black. Rows are MSB-first. */
#ifndef FB_H_INCLUDED
#define FB_H_INCLUDED

#include <stdint.h>

#define FB_W        264
#define FB_H        176
#define FB_STRIDE   (FB_W / 8)
#define FB_BYTES    (FB_STRIDE * FB_H)

struct glyph {
    uint32_t cp;            /* Unicode code point */
    uint8_t  w, h;          /* bitmap size */
    int8_t   dx, dy;        /* bitmap offset from the pen position (dy from the top of the line) */
    uint8_t  adv;           /* pen advance */
    const uint8_t *bits;    /* rows of (w + 7) / 8 bytes, MSB first */
};

struct font {
    uint8_t line_h;
    uint16_t count;
    const struct glyph *glyphs;     /* sorted by cp */
};

struct fb_rect { int16_t x, y, w, h; };

void fb_clear(uint8_t *fb);
void fb_pixel(uint8_t *fb, int x, int y, int black);
void fb_hline(uint8_t *fb, int x0, int x1, int y, int black);
void fb_vline(uint8_t *fb, int x, int y0, int y1, int black);
void fb_line(uint8_t *fb, int x0, int y0, int x1, int y1, int black);
void fb_rect(uint8_t *fb, int x, int y, int w, int h, int black);
void fb_fill(uint8_t *fb, int x, int y, int w, int h, int black);
void fb_circle(uint8_t *fb, int cx, int cy, int r, int black);
void fb_disc(uint8_t *fb, int cx, int cy, int r, int black);

/* Draw UTF-8 text with its top-left at (x, y). Returns the pen x after the text. */
int  fb_text(uint8_t *fb, const struct font *f, int x, int y, const char *s, int black);
int  fb_text_width(const struct font *f, const char *s);

/*
 * Compare two frames and return up to `max` rectangles covering every changed pixel.
 * Rectangles are byte-aligned horizontally (multiples of 8 px), which suits e-ink partial updates.
 */
int  fb_diff(const uint8_t *a, const uint8_t *b, struct fb_rect *out, int max);

#endif
