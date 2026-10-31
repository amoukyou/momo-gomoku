/*
 * Tethered test: loaded into SRAM over USB in the ROM's download mode and run from there.
 * Nothing is written to flash; a power cycle brings back the stock firmware.
 *
 * The ROM calls entry() with a pointer to its loader argument block. Bits of `arg`:
 *   bit 0..1  e-ink orientation (EPD_FLIP_X / EPD_FLIP_Y)
 * Picking "power off" in the menu resets the chip, which boots the stock firmware again.
 */
#include <string.h>
#include "app.h"
#include "boot_anim.h"
#include "epd_ssd1680.h"
#include "fb.h"
#include "hal.h"

struct loader_args {
    void (*msd_send)(void *, int);
    void (*msd_recv)(void *, int);
    void *msd_hook;
    uint32_t arg;
    uint32_t wtw;
};

extern char __bss_start[], __bss_end[];

static uint8_t fb[FB_BYTES], shown[FB_BYTES];
static int partials;
static uint32_t ticks;

static void show(int force_full)
{
    app_render(fb);
    if (force_full || app_wants_full_refresh() || partials >= 20) {
        epd_full(fb);
        partials = 0;
    } else if (memcmp(fb, shown, FB_BYTES) != 0) {
        epd_partial(fb);
        partials++;
    }
    memcpy(shown, fb, FB_BYTES);
}

/* How long (in 10 ms ticks) a key stays down, up to `limit`. */
static int hold_time(unsigned key, int limit)
{
    int held = 0;
    while ((hal_keys() & key) && held < limit) { hal_delay_ms(10); held++; }
    return held;
}

/* Turn one buffered key press into an app key. Short big round = OK, short small round = undo,
 * a long press of either = menu. Returns -1 if nothing is queued. */
static int take_key(void)
{
    unsigned ev = hal_key_event();
    switch (ev) {
    case HK_UP:    return KEY_UP;
    case HK_DOWN:  return KEY_DOWN;
    case HK_LEFT:  return KEY_LEFT;
    case HK_RIGHT: return KEY_RIGHT;
    case HK_POWER: return KEY_MENU;
    case HK_OK:
    case HK_BACK:
        if (hold_time(ev, 80) < 80)
            return ev == HK_OK ? KEY_OK : KEY_UNDO;
        while (hal_keys() & ev) hal_delay_ms(10);
        return KEY_MENU;
    }
    return -1;
}

static int next_key(void)
{
    int k;
    while ((k = take_key()) < 0) {
        hal_delay_ms(10);
        ticks++;
    }
    return k;
}

/* Boot animation, then wait for any key. */
static void play_boot(void)
{
    epd_full(boot_anim_frames[0]);
    partials = 0;
    for (int i = 1; i < BOOT_ANIM_FRAMES; i++) {
        hal_delay_ms(120);
        epd_partial(boot_anim_frames[i]);
    }
    memcpy(shown, boot_anim_frames[BOOT_ANIM_FRAMES - 1], FB_BYTES);
    while (hal_key_event()) {}                  /* ignore presses made during the animation */
    while (!hal_key_event()) { hal_delay_ms(10); ticks++; }
}

__attribute__((section(".text.entry")))
void entry(struct loader_args *la)
{
    memset(__bss_start, 0, (size_t)(__bss_end - __bss_start));
    uint32_t arg = la ? la->arg : 0;

    hal_init();
    epd_set_orientation((int)(arg & 3));

    play_boot();

    app_init(ticks * 2654435761u + 1);
    show(1);
    for (;;) {
        /* handle every press that queued up, then refresh the screen once */
        int changed = app_key((enum app_key)next_key());
        for (int k; !app_busy() && !app_wants_poweroff() && (k = take_key()) >= 0;)
            changed |= app_key((enum app_key)k);
        if (changed)
            show(0);
        if (app_wants_replay()) {
            play_boot();
            show(1);
            continue;
        }
        if (app_wants_poweroff()) {
            epd_sleep();
            hal_reset();        /* the tethered build can't power off; hand back to the stock firmware */
        }
        while (app_busy()) {
            app_step();
            show(0);
        }
    }
}
