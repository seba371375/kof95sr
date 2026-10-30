#include "ng_internal.h"
#include "osd_menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    uint32_t *fb = (uint32_t *)calloc(NG_SCREEN_W * NG_SCREEN_H, sizeof(uint32_t));

    // Fill with a nice gradient background resembling attract mode
    for (int y = 0; y < NG_SCREEN_H; y++) {
        for (int x = 0; x < NG_SCREEN_W; x++) {
            uint8_t r = (uint8_t)(30 + (x * 40 / NG_SCREEN_W));
            uint8_t g = (uint8_t)(40 + (y * 50 / NG_SCREEN_H));
            uint8_t b = (uint8_t)(80 + ((x + y) * 40 / (NG_SCREEN_W + NG_SCREEN_H)));
            fb[y * NG_SCREEN_W + x] = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
    }

    // Set sample DIP state (DIP 7 Free Play ON, DIP 2 ON)
    io_set_dipswitch_bit(1, 1);
    io_set_dipswitch_bit(6, 1);

    // Open OSD menu and render over framebuffer
    osd_menu_toggle();
    osd_menu_render(fb);

    write_png("re/out/osd_menu_preview.png", fb, NG_SCREEN_W, NG_SCREEN_H);
    printf("Saved re/out/osd_menu_preview.png successfully\n");
    free(fb);
    return 0;
}
