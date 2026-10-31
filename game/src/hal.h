/*
 * Board hardware for the Baicizhan Ace-1S (Jieli AC7916AB), worked out from the stock firmware:
 *   e-ink (SSD1680, 176x264): SCK PC09, MOSI PC10, CS PC06, DC PC07, RST PC08, BUSY PC05 (high = busy)
 *   keys (active low):        UP PC01, DOWN PA07, LEFT PH03, RIGHT PC00,
 *                             big round key PA08 (confirm), small round key PA10
 *   ADC keys on PB01:         top power key ~0, side volume+ ~588, volume- ~693 (of 1023)
 *   e-ink is mounted upside down relative to the RAM layout: use EPD_FLIP_X | EPD_FLIP_Y
 */
#ifndef HAL_H
#define HAL_H

#include <stdint.h>

enum {
    HK_UP = 1 << 0, HK_DOWN = 1 << 1, HK_LEFT = 1 << 2, HK_RIGHT = 1 << 3,
    HK_OK = 1 << 4,         /* big round key */
    HK_BACK = 1 << 5,       /* small round key */
    HK_POWER = 1 << 6,      /* top key */
};

void hal_init(void);
void hal_delay_ms(int ms);
unsigned hal_keys(void);          /* bitmask of HK_* currently held (debounced) */
unsigned hal_key_event(void);     /* next buffered key press (one HK_* bit), or 0 if none */
void hal_idle(void);              /* call often during long work (e.g. screen refresh): samples the keys */
void hal_reset(void);             /* software reset: boots the firmware in flash */

/* e-ink pins */
void hal_epd_cs(int v);
void hal_epd_dc(int v);
void hal_epd_rst(int v);
void hal_epd_sck(int v);
void hal_epd_mosi(int v);
int  hal_epd_busy(void);

#endif
