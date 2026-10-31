/* Animation frames, generated from art/boot_anim.html. Each frame is a full 264x176 1-bit image. */
#ifndef BOOT_ANIM_H
#define BOOT_ANIM_H

#include <stdint.h>

#define BOOT_ANIM_FRAMES    6       /* power-on: stones drop, Momo pops up, title */
#define SLEEP_ANIM_FRAMES   4       /* lock: Momo yawns and falls asleep; the last one is the lock screen */
#define OFF_FRAMES          1       /* power-off still */

extern const uint8_t *const boot_anim_frames[BOOT_ANIM_FRAMES];
extern const uint8_t *const sleep_anim_frames[SLEEP_ANIM_FRAMES];
extern const uint8_t *const off_frames[OFF_FRAMES];

#endif
