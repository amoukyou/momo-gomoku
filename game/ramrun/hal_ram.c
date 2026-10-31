/*
 * Bare-metal HAL for running from SRAM in the ROM's USB download mode (no SDK, no OS).
 * WL82 GPIO: one register block per port at 0x50000 + 0x40 * port. DIR bit 1 = input.
 */
#include "hal.h"

#define PORT_BASE(p)    (0x50000u + 0x40u * (p))
#define REG(p, off)     (*(volatile uint32_t *)(PORT_BASE(p) + (off)))
#define OUT  0x00
#define IN   0x04
#define DIR  0x08
#define DIE  0x0c
#define PU   0x10
#define PD   0x14

enum { PA, PB, PC, PD_, PE, PF, PG, PH };

static void pin_output(int port, int bit, int v)
{
    if (v) REG(port, OUT) |= 1u << bit; else REG(port, OUT) &= ~(1u << bit);
    REG(port, DIR) &= ~(1u << bit);
}

static void pin_input(int port, int bit, int pull_up)
{
    REG(port, DIR) |= 1u << bit;
    REG(port, DIE) |= 1u << bit;
    REG(port, PD) &= ~(1u << bit);
    if (pull_up) REG(port, PU) |= 1u << bit; else REG(port, PU) &= ~(1u << bit);
}

static inline void pin_set(int port, int bit, int v)
{
    if (v) REG(port, OUT) |= 1u << bit; else REG(port, OUT) &= ~(1u << bit);
}

static inline int pin_get(int port, int bit)
{
    return (REG(port, IN) >> bit) & 1;
}

void hal_init(void)
{
    pin_output(PC, 6, 1);   /* CS */
    pin_output(PC, 7, 1);   /* DC */
    pin_output(PC, 8, 1);   /* RST */
    pin_output(PC, 9, 0);   /* SCK */
    pin_output(PC, 10, 0);  /* MOSI */
    pin_input(PC, 5, 0);    /* BUSY */

    pin_input(PC, 1, 1);    /* UP */
    pin_input(PA, 7, 1);    /* DOWN */
    pin_input(PH, 3, 1);    /* LEFT */
    pin_input(PC, 0, 1);    /* RIGHT */
    pin_input(PA, 8, 1);    /* big round key */
    pin_input(PA, 10, 1);   /* small round key */
    pin_input(PB, 1, 0);    /* ADC key line, has an external pull-up; the top power key pulls it to 0 V */
}

/*
 * The watchdog lives in the P33 (always-on) domain and is reached through a small serial bus:
 * CON at 0x13e08, data at 0x13e0c. Feeding it = OR 0x40 into P33 register 0x80 (as the SDK's
 * wdt_clear() does). The ROM feeds it while it waits for USB commands; our code has to as well.
 */
#define P33_CON (*(volatile uint32_t *)0x13e08)
#define P33_BUF (*(volatile uint32_t *)0x13e0c)

static void p33_xfer(uint8_t b)
{
    P33_BUF = b;
    P33_CON |= 0x10;
    while (P33_CON & 2) {}
}

static void wdt_clear(void)
{
    P33_CON &= ~0x100u;
    P33_CON |= 1;
    p33_xfer(0x20);     /* OR, address bits 9:8 = 0 */
    p33_xfer(0x80);     /* WDT_CON */
    p33_xfer(0x40);     /* clear bit */
    P33_CON &= ~0x101u;
}

/* rough busy-wait; the ROM's clock speed is not known exactly, so err on the long side */
void hal_delay_ms(int ms)
{
    while (ms-- > 0) {
        wdt_clear();
        hal_idle();
        for (volatile int i = 0; i < 24000; i++) {}
    }
}

static unsigned raw_keys(void)
{
    unsigned k = 0;
    if (!pin_get(PC, 1))  k |= HK_UP;
    if (!pin_get(PA, 7))  k |= HK_DOWN;
    if (!pin_get(PH, 3))  k |= HK_LEFT;
    if (!pin_get(PC, 0))  k |= HK_RIGHT;
    if (!pin_get(PA, 8))  k |= HK_OK;
    if (!pin_get(PA, 10)) k |= HK_BACK;
    if (!pin_get(PB, 1))  k |= HK_POWER;   /* the top key pulls the ADC key line to 0 V */
    return k;
}

/*
 * Debounce: a key's state only changes after it reads the same for DEBOUNCE samples in a row.
 * Every new press is queued, so presses made while the screen refreshes are not lost.
 */
#define DEBOUNCE 4
static unsigned stable;
static uint8_t same_count[8];
static uint8_t queue[16];
static unsigned q_head, q_tail;

void hal_idle(void)
{
    unsigned now = raw_keys();
    for (int b = 0; b < 7; b++) {
        unsigned m = 1u << b;
        if ((now & m) == (stable & m)) {
            same_count[b] = 0;
            continue;
        }
        if (++same_count[b] < DEBOUNCE)
            continue;
        same_count[b] = 0;
        stable ^= m;
        if ((stable & m) && q_head - q_tail < sizeof(queue))
            queue[q_head++ % sizeof(queue)] = (uint8_t)b;
    }
}

unsigned hal_keys(void)
{
    hal_idle();
    return stable;
}

unsigned hal_key_event(void)
{
    hal_idle();
    if (q_tail == q_head)
        return 0;
    return 1u << queue[q_tail++ % sizeof(queue)];
}

void hal_reset(void)
{
    *(volatile uint32_t *)0x10000 |= 1u << 4;     /* JL_CLOCK->PWR_CON: system reset */
    for (;;) {}
}

void hal_epd_cs(int v)   { pin_set(PC, 6, v); }
void hal_epd_dc(int v)   { pin_set(PC, 7, v); }
void hal_epd_rst(int v)  { pin_set(PC, 8, v); }
void hal_epd_sck(int v)  { pin_set(PC, 9, v); }
void hal_epd_mosi(int v) { pin_set(PC, 10, v); }
int  hal_epd_busy(void)  { return pin_get(PC, 5); }
