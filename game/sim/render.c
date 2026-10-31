/*
 * Host preview: feed a key script to the app and dump each frame as raw 1-bit data.
 * usage: render <outdir> <keys>   keys: u d l r o (ok) z (undo) m (menu), '.' = snapshot
 */
#include <stdio.h>
#include "../src/app.h"
#include "../src/fb.h"

static uint8_t fb[FB_BYTES];

static void snap(const char *dir, int n)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/frame%02d.raw", dir, n);
    FILE *f = fopen(path, "wb");
    fwrite(fb, 1, FB_BYTES, f);
    fclose(f);
}

int main(int argc, char **argv)
{
    int n = 0;
    app_init(42);
    app_render(fb);
    snap(argv[1], n++);
    for (const char *k = argv[2]; *k; k++) {
        enum app_key key;
        switch (*k) {
        case 'u': key = KEY_UP; break;
        case 'd': key = KEY_DOWN; break;
        case 'l': key = KEY_LEFT; break;
        case 'r': key = KEY_RIGHT; break;
        case 'o': key = KEY_OK; break;
        case 'z': key = KEY_UNDO; break;
        case 'm': key = KEY_MENU; break;
        case '.': app_render(fb); snap(argv[1], n++); continue;
        default: continue;
        }
        app_key(key);
        while (app_busy()) app_step();
    }
    app_render(fb);
    snap(argv[1], n++);
    return 0;
}
