#ifndef OSD_MENU_H
#define OSD_MENU_H

#include "ng_internal.h"
#include <SDL.h>

/* Returns true if the DIP switch menu overlay is currently active */
bool osd_menu_is_open(void);

/* Toggles menu visibility */
void osd_menu_toggle(void);

/* Handles SDL keyboard/window events. Returns true if consumed by menu */
bool osd_menu_handle_event(const SDL_Event *e);

/* Updates gamepad navigation when menu is open */
void osd_menu_update_gamepad(SDL_GameController *pad);

/* Renders the OSD menu over the 320x224 ARGB framebuffer */
void osd_menu_render(uint32_t *fb);

/* Controller layout preset (0=SNK Arcade, 1=Fighting Standard, 2=KOF Classic Line) */
int  window_get_pad_layout(void);
void window_set_pad_layout(int layout);
const char *window_get_pad_layout_name(int layout);

/* Video filter preset (0=Nearest, 1=Bilinear, 2=CRT 50%, 3=CRT 75%, 4=Trinitron, 5=Scale2x, 6=CRT Warm) */
int  window_get_video_filter(void);
void window_set_video_filter(int filter);
const char *window_get_video_filter_name(int filter);

/* Keyboard configuration actions */
typedef enum {
    KEY_ACT_P1_UP = 0,
    KEY_ACT_P1_DOWN,
    KEY_ACT_P1_LEFT,
    KEY_ACT_P1_RIGHT,
    KEY_ACT_P1_A,
    KEY_ACT_P1_B,
    KEY_ACT_P1_C,
    KEY_ACT_P1_D,
    KEY_ACT_START1,
    KEY_ACT_COIN1,
    KEY_ACT_P2_UP,
    KEY_ACT_P2_DOWN,
    KEY_ACT_P2_LEFT,
    KEY_ACT_P2_RIGHT,
    KEY_ACT_P2_A,
    KEY_ACT_P2_B,
    KEY_ACT_P2_C,
    KEY_ACT_P2_D,
    KEY_ACT_START2,
    KEY_ACT_COIN2,
    NUM_KEY_ACTIONS
} key_action_t;

SDL_Scancode window_get_key_mapping(key_action_t action);
void         window_set_key_mapping(key_action_t action, SDL_Scancode code);
const char  *window_get_key_action_name(key_action_t action);
const char  *window_get_key_name(SDL_Scancode code);
void         window_reset_default_keys(void);
void         window_save_controls_cfg(void);

#endif /* OSD_MENU_H */
