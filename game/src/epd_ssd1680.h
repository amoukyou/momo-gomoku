/* SSD1680 e-ink driver (2.7", 176x264 panel used in landscape as 264x176). */
#ifndef EPD_SSD1680_H
#define EPD_SSD1680_H

#include <stdint.h>

/* orientation bits, to match how the panel is mounted */
#define EPD_FLIP_X  1     /* mirror horizontally (in landscape) */
#define EPD_FLIP_Y  2     /* mirror vertically (in landscape) */

void epd_set_orientation(int flags);

/* Full refresh with the usual e-ink flashing; clears ghosting. fb is a 264x176 1-bit frame (1 = black). */
void epd_full(const uint8_t *fb);

/* Partial refresh without flashing (mode 2 waveform). */
void epd_partial(const uint8_t *fb);

/* Put the controller into deep sleep; the next epd_full() wakes it up again. */
void epd_sleep(void);

#endif
