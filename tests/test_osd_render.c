#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#define NG_SCREEN_W 320
#define NG_SCREEN_H 224

#ifdef __cplusplus
extern "C" {
#endif
bool osd_menu_is_open(void);
void osd_menu_toggle(void);
void osd_menu_render(uint32_t *fb);
int  window_get_pad_layout(void);
void window_set_pad_layout(int layout);
const char *window_get_pad_layout_name(int layout);
int  write_png(const char *path, const uint32_t *argb, int w, int h);
#ifdef __cplusplus
}
#endif

int main(void) {
    uint32_t fb[NG_SCREEN_W * NG_SCREEN_H];
    for (int i = 0; i < NG_SCREEN_W * NG_SCREEN_H; i++) {
        fb[i] = 0xFF102030; // dark blue background
    }

    printf("Default pad layout: %d (%s)\n", window_get_pad_layout(), window_get_pad_layout_name(window_get_pad_layout()));
    osd_menu_toggle(); // Open menu
    printf("Menu is open: %d\n", osd_menu_is_open());

    osd_menu_render(fb);
    write_png("test_osd_menu.png", fb, NG_SCREEN_W, NG_SCREEN_H);
    printf("Saved test_osd_menu.png\n");

    // Test cycle layout
    window_set_pad_layout(1);
    printf("Changed layout to 1: %s\n", window_get_pad_layout_name(window_get_pad_layout()));
    osd_menu_render(fb);
    write_png("test_osd_menu_layout1.png", fb, NG_SCREEN_W, NG_SCREEN_H);

    return 0;
}
