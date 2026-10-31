/*
 * SSD1680 driver, bit-banged SPI. The command sequences follow the stock Ace-1S firmware:
 * full refresh writes both RAMs and uses 0x22=0xF7; partial refresh sets the border to 0x80,
 * powers up with 0x22=0xC0, writes the new image and updates with 0x22=0x3C (mode 2).
 */
#include "epd_ssd1680.h"
#include "fb.h"
#include "hal.h"

#define PANEL_W     176     /* source lines (RAM X) */
#define PANEL_H     264     /* gate lines (RAM Y) */

static int orient;
static int awake;

void epd_set_orientation(int flags) { orient = flags; }

static void spi_byte(uint8_t b)
{
    for (int i = 0; i < 8; i++) {
        hal_epd_mosi(b & 0x80);
        hal_epd_sck(1);
        b <<= 1;
        hal_epd_sck(0);
    }
}

static void cmd(uint8_t c)
{
    hal_epd_dc(0);
    hal_epd_cs(0);
    spi_byte(c);
    hal_epd_cs(1);
}

static void data(uint8_t d)
{
    hal_epd_dc(1);
    hal_epd_cs(0);
    spi_byte(d);
    hal_epd_cs(1);
}

static void wait_busy(void)
{
    /* full refresh takes ~3 s; give up after ~10 s rather than hang forever */
    for (int i = 0; i < 2000 && hal_epd_busy(); i++)
        hal_delay_ms(5);
}

static void init_panel(void)
{
    hal_epd_cs(1);
    hal_epd_sck(0);
    hal_epd_rst(1);
    hal_delay_ms(10);
    hal_epd_rst(0);
    hal_delay_ms(10);
    hal_epd_rst(1);
    hal_delay_ms(10);
    wait_busy();

    cmd(0x12);                          /* software reset */
    wait_busy();
    cmd(0x01);                          /* driver output control: 264 gates */
    data((PANEL_H - 1) & 0xff); data((PANEL_H - 1) >> 8); data(0x00);
    cmd(0x11); data(0x03);              /* data entry: X+, Y+ */
    cmd(0x44); data(0x00); data(PANEL_W / 8 - 1);
    cmd(0x45); data(0x00); data(0x00); data((PANEL_H - 1) & 0xff); data((PANEL_H - 1) >> 8);
    cmd(0x3C); data(0x05);              /* border waveform */
    cmd(0x18); data(0x80);              /* internal temperature sensor */
    wait_busy();
    awake = 1;
}

/* Send a landscape 264x176 frame as the panel's portrait 176x264 RAM image. */
static void write_ram(uint8_t reg, const uint8_t *fb)
{
    cmd(0x4E); data(0x00);
    cmd(0x4F); data(0x00); data(0x00);
    cmd(reg);
    hal_epd_dc(1);
    hal_epd_cs(0);
    for (int y = 0; y < PANEL_H; y++) {
        hal_idle();         /* keep catching key presses while the frame streams out */
        int fx = (orient & EPD_FLIP_X) ? FB_W - 1 - y : y;
        for (int xb = 0; xb < PANEL_W / 8; xb++) {
            uint8_t out = 0;
            for (int bit = 0; bit < 8; bit++) {
                int x = xb * 8 + bit;
                int fy = (orient & EPD_FLIP_Y) ? x : FB_H - 1 - x;
                int black = fb[fy * FB_STRIDE + (fx >> 3)] & (0x80 >> (fx & 7));
                if (!black)
                    out |= (uint8_t)(0x80 >> bit);     /* panel RAM: 1 = white */
            }
            spi_byte(out);
        }
    }
    hal_epd_cs(1);
}

static void update(uint8_t mode)
{
    cmd(0x22); data(mode);
    cmd(0x20);
    wait_busy();
}

void epd_full(const uint8_t *fb)
{
    init_panel();
    write_ram(0x24, fb);
    write_ram(0x26, fb);
    update(0xF7);
}

void epd_partial(const uint8_t *fb)
{
    if (!awake) {
        epd_full(fb);
        return;
    }
    cmd(0x3C); data(0x80);
    update(0xC0);
    write_ram(0x24, fb);
    update(0x3C);
}

void epd_sleep(void)
{
    if (!awake)
        return;
    cmd(0x10); data(0x01);
    awake = 0;
}
