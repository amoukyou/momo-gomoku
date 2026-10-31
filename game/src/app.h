/* Gomoku app: menu, board, input handling and rendering. Hardware independent. */
#ifndef APP_H
#define APP_H

#include <stdint.h>

/* KEY_MENU opens the pause menu in a game (long press of the round key) */
enum app_key { KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_OK, KEY_UNDO, KEY_MENU };

void app_init(uint32_t seed);

/* Handle one key press. Returns 1 if the screen changed. */
int  app_key(enum app_key k);

/* True once the player picked "power off" in the main menu. */
int  app_wants_poweroff(void);

/* True (once) after the player picked "start over": show the boot animation again, then the menu. */
int  app_wants_replay(void);

/* True while the computer still has to make its move; call app_step() after showing the frame. */
int  app_busy(void);
void app_step(void);

/* True when the next frame should be a full refresh (screen change), to clear e-ink ghosting. */
int  app_wants_full_refresh(void);

/* Draw the whole current frame into a 1-bit 264x176 framebuffer. */
void app_render(uint8_t *fb);

#endif
