/*
 * osd_menu.c — On-Screen Display (OSD) Menu for editing Neo Geo DIP switches.
 *
 * Rendered directly onto the 320x224 ARGB framebuffer so it works identically
 * across all display backends (SDL Accelerated, OpenGL, Vulkan, and PS5).
 *
 * Controls:
 *   - Open/Close: Tab, F1, or Select+Start on Gamepad.
 *   - Navigate: Up/Down.
 *   - Toggle / Action: Enter, Space, Left/Right, or Cross (A) on Gamepad.
 *   - Back / Save: Esc, Tab, or Circle (B) on Gamepad.
 */
#include "osd_menu.h"
#include "osd_font.h"
#include <stdio.h>
#include <string.h>

static bool s_menu_open = false;
static int  s_cursor = 0;
static const char *s_cfg_path = "kof95_dips.cfg";

typedef enum {
    PAGE_MAIN = 0,
    PAGE_KEYBOARD
} menu_page_t;

static menu_page_t s_page = PAGE_MAIN;
static int s_key_cursor = 0;
static bool s_waiting_key = false;

#define NUM_MAIN_ITEMS 18

static const struct {
    const char *label;
    const char *desc;
} s_main_items[NUM_MAIN_ITEMS] = {
    {"Game Difficulty",                "Difficulty: Level 1 (Easy) to 8"},
    {"Blood Color",                    "Arcade Blood: ON (Red) or OFF"},
    {"Duplicate Characters",           "Allow duplicate fighters in Team Edit"},
    {"Random Select (?)",              "Enable (?) random character slot on grid"},
    {"Gamepad Layout",                 "Button Mapping preset (SNK/Modern/Line)"},
    {"Video Filter",                   "Shader/Filter (Raw/Bilinear/CRT/Scale2x)"},
    {"VSync (Latency)",                "VSync: ON (Smooth) or OFF (Lowest Lag)"},
    {"[ Configure Keyboard Keys... ]", "Remap Player 1 & 2 keyboard keys"},
    {"DIP 1: Test / Setting Mode",     "Enter MVS Hardware Test Menu"},
    {"DIP 2: Coin Chute 2 Option",     "Select 1 coin chute or 2 chutes"},
    {"DIP 3: Controller Type",         "Joystick vs Mahjong controller"},
    {"DIP 4: Multi-Link Comm",         "Multi-cabinet link communication"},
    {"DIP 5: Demo Sound / Option",     "Demo sound audio toggle option"},
    {"DIP 6: Screen Stop / Flip",      "Screen freeze or flip output"},
    {"DIP 7: Free Play Mode",          "Free Play mode: no coins needed"},
    {"DIP 8: Hardware Freeze",         "Hardware pause: freeze gameplay"},
    {"[ Enter BIOS Test Menu (F2) ]",  "Boot MVS Hardware Test screen"},
    {"[ Save & Close Menu (TAB/ESC) ]","Save configuration and resume"}
};

/* Keyboard config page has 20 key actions + 2 controls (Reset Defaults, Back) = 22 items */
#define KEY_MENU_RESET (NUM_KEY_ACTIONS)
#define KEY_MENU_BACK  (NUM_KEY_ACTIONS + 1)
#define NUM_KEY_MENU_ITEMS (NUM_KEY_ACTIONS + 2)

bool osd_menu_is_open(void) {
    return s_menu_open;
}

void osd_menu_toggle(void) {
    s_menu_open = !s_menu_open;
    if (!s_menu_open) {
        s_page = PAGE_MAIN;
        s_waiting_key = false;
        io_save_dipswitches(s_cfg_path);
        window_save_controls_cfg();
    }
}

static void main_menu_change(int delta) {
    if (s_cursor == 0) {
        int diff = (int)io_get_difficulty();
        diff += delta;
        if (diff < 1) diff = 8;
        if (diff > 8) diff = 1;
        io_set_difficulty((uint8_t)diff);
    } else if (s_cursor == 1) {
        io_set_blood(!io_get_blood());
    } else if (s_cursor == 2) {
        io_set_allow_duplicates(!io_get_allow_duplicates());
    } else if (s_cursor == 3) {
        io_set_random_select(!io_get_random_select());
    } else if (s_cursor == 4) {
        int cur = window_get_pad_layout();
        window_set_pad_layout(cur + delta);
    } else if (s_cursor == 5) {
        int cur = window_get_video_filter();
        window_set_video_filter(cur + delta);
    } else if (s_cursor == 6) {
        window_set_vsync(!window_get_vsync());
    } else if (s_cursor == 7) {
        // Enter Keyboard Config page
        s_page = PAGE_KEYBOARD;
        s_key_cursor = 0;
        s_waiting_key = false;
    } else if (s_cursor >= 8 && s_cursor <= 15) {
        int bit = s_cursor - 8;
        uint8_t dips = io_get_dipswitches();
        int cur = (dips >> bit) & 1;
        io_set_dipswitch_bit(bit, !cur);
    } else if (s_cursor == 16) {
        uint8_t dips = io_get_dipswitches();
        io_set_dipswitch_bit(0, !(dips & 1));
        osd_menu_toggle();
    } else if (s_cursor == 17) {
        osd_menu_toggle();
    }
}

static void key_menu_action(void) {
    if (s_key_cursor < NUM_KEY_ACTIONS) {
        s_waiting_key = true;
    } else if (s_key_cursor == KEY_MENU_RESET) {
        window_reset_default_keys();
    } else if (s_key_cursor == KEY_MENU_BACK) {
        s_page = PAGE_MAIN;
        s_waiting_key = false;
    }
}

bool osd_menu_handle_event(const SDL_Event *e) {
    if (e->type == SDL_KEYDOWN) {
        SDL_Keycode k = e->key.keysym.sym;
        SDL_Scancode sc = e->key.keysym.scancode;

        // If waiting for a key to rebind
        if (s_menu_open && s_page == PAGE_KEYBOARD && s_waiting_key) {
            if (sc == SDL_SCANCODE_ESCAPE) {
                // Cancel rebinding
                s_waiting_key = false;
                return true;
            }
            if (s_key_cursor < NUM_KEY_ACTIONS) {
                window_set_key_mapping((key_action_t)s_key_cursor, sc);
                s_waiting_key = false;
                return true;
            }
        }

        if (k == SDLK_TAB || k == SDLK_F1) {
            osd_menu_toggle();
            return true;
        }
        if (!s_menu_open) return false;

        if (k == SDLK_ESCAPE) {
            if (s_page == PAGE_KEYBOARD) {
                s_page = PAGE_MAIN;
                return true;
            }
            osd_menu_toggle();
            return true;
        }

        if (s_page == PAGE_MAIN) {
            if (k == SDLK_UP) {
                s_cursor = (s_cursor - 1 + NUM_MAIN_ITEMS) % NUM_MAIN_ITEMS;
                return true;
            } else if (k == SDLK_DOWN) {
                s_cursor = (s_cursor + 1) % NUM_MAIN_ITEMS;
                return true;
            } else if (k == SDLK_LEFT) {
                main_menu_change(-1);
                return true;
            } else if (k == SDLK_RIGHT) {
                main_menu_change(1);
                return true;
            } else if (k == SDLK_RETURN || k == SDLK_SPACE) {
                main_menu_change(1);
                return true;
            }
        } else if (s_page == PAGE_KEYBOARD) {
            if (k == SDLK_UP) {
                s_key_cursor = (s_key_cursor - 1 + NUM_KEY_MENU_ITEMS) % NUM_KEY_MENU_ITEMS;
                return true;
            } else if (k == SDLK_DOWN) {
                s_key_cursor = (s_key_cursor + 1) % NUM_KEY_MENU_ITEMS;
                return true;
            } else if (k == SDLK_RETURN || k == SDLK_SPACE) {
                key_menu_action();
                return true;
            }
        }
        return true; // Consume other keys while menu is open
    }
    return false;
}

static uint32_t s_last_pad_tick = 0;

void osd_menu_update_gamepad(SDL_GameController *pad) {
    if (!pad) return;

    bool back = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK) ||
                SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_TOUCHPAD);
    bool start = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START);
    bool thumb_combo = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSTICK) &&
                       SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSTICK);
    bool shoulder_start = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) &&
                          SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) &&
                          start;

    uint32_t now = SDL_GetTicks();
    if (((back && start) || thumb_combo || shoulder_start) && (now - s_last_pad_tick > 400)) {
        s_last_pad_tick = now;
        osd_menu_toggle();
        return;
    }

    if (!s_menu_open) return;

    if (now - s_last_pad_tick < 180) return; // Debounce rate

    if (s_page == PAGE_MAIN) {
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP)) {
            s_cursor = (s_cursor - 1 + NUM_MAIN_ITEMS) % NUM_MAIN_ITEMS;
            s_last_pad_tick = now;
        } else if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) {
            s_cursor = (s_cursor + 1) % NUM_MAIN_ITEMS;
            s_last_pad_tick = now;
        } else if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) {
            main_menu_change(-1);
            s_last_pad_tick = now;
        } else if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) {
            main_menu_change(1);
            s_last_pad_tick = now;
        } else if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A)) {
            main_menu_change(1);
            s_last_pad_tick = now;
        } else if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B) ||
                   SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START)) {
            osd_menu_toggle();
            s_last_pad_tick = now;
        }
    } else if (s_page == PAGE_KEYBOARD) {
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP)) {
            s_key_cursor = (s_key_cursor - 1 + NUM_KEY_MENU_ITEMS) % NUM_KEY_MENU_ITEMS;
            s_last_pad_tick = now;
        } else if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) {
            s_key_cursor = (s_key_cursor + 1) % NUM_KEY_MENU_ITEMS;
            s_last_pad_tick = now;
        } else if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A)) {
            key_menu_action();
            s_last_pad_tick = now;
        } else if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B)) {
            s_page = PAGE_MAIN;
            s_waiting_key = false;
            s_last_pad_tick = now;
        }
    }
}

/* ---- Software Rendering for OSD ---- */

static inline uint32_t blend_pixel(uint32_t bg, uint32_t fg, uint8_t a) {
    uint32_t rb = bg & 0x00FF00FF;
    uint32_t g  = bg & 0x0000FF00;
    uint32_t frb = fg & 0x00FF00FF;
    uint32_t fg_ = fg & 0x0000FF00;

    rb += ((frb - rb) * a) >> 8;
    g  += ((fg_ - g) * a) >> 8;

    return 0xFF000000 | (rb & 0x00FF00FF) | (g & 0x0000FF00);
}

static void draw_box(uint32_t *fb, int bx, int by, int bw, int bh, uint32_t fill_color, uint32_t border_color) {
    uint8_t a = (uint8_t)(fill_color >> 24);
    for (int y = by; y < by + bh; y++) {
        if (y < 0 || y >= NG_SCREEN_H) continue;
        for (int x = bx; x < bx + bw; x++) {
            if (x < 0 || x >= NG_SCREEN_W) continue;
            if (x == bx || x == bx + bw - 1 || y == by || y == by + bh - 1) {
                fb[y * NG_SCREEN_W + x] = border_color;
            } else {
                fb[y * NG_SCREEN_W + x] = blend_pixel(fb[y * NG_SCREEN_W + x], fill_color, a);
            }
        }
    }
}

static void draw_char(uint32_t *fb, int cx, int cy, char ch, uint32_t color) {
    if (ch < 32 || ch > 126) ch = '?';
    const uint8_t *bitmap = font_8x8[(int)(ch - 32)];

    for (int r = 0; r < 8; r++) {
        int py = cy + r;
        if (py < 0 || py >= NG_SCREEN_H) continue;
        uint8_t row = bitmap[r];
        for (int c = 0; c < 8; c++) {
            int px = cx + c;
            if (px < 0 || px >= NG_SCREEN_W) continue;
            if ((row >> (7 - c)) & 1) {
                // Drop shadow
                if (px + 1 < NG_SCREEN_W && py + 1 < NG_SCREEN_H) {
                    fb[(py + 1) * NG_SCREEN_W + (px + 1)] = 0xFF050505;
                }
                fb[py * NG_SCREEN_W + px] = color;
            }
        }
    }
}

static void draw_text(uint32_t *fb, int x, int y, const char *str, uint32_t color) {
    while (*str) {
        draw_char(fb, x, y, *str++, color);
        x += 8;
    }
}

void osd_menu_render(uint32_t *fb) {
    if (!s_menu_open || !fb) return;

    // Outer Dialog Box (Center screen: 312x216 inside 320x224)
    int box_x = 4;
    int box_y = 4;
    int box_w = 312;
    int box_h = 216;

    draw_box(fb, box_x, box_y, box_w, box_h, 0xDC0C1424, 0xFFE5B022);

    if (s_page == PAGE_MAIN) {
        // Title
        draw_text(fb, box_x + 48, box_y + 6, "KOF95 CONFIG / DIP SWITCHES", 0xFFFFD700);

        // Divider line
        for (int x = box_x + 6; x < box_x + box_w - 6; x++) {
            fb[(box_y + 17) * NG_SCREEN_W + x] = 0xFF556677;
        }

        uint8_t dips = io_get_dipswitches();

        // Menu Entries (17 items spaced at 9px intervals: start_y = 20)
        int start_y = box_y + 19;
        for (int i = 0; i < NUM_MAIN_ITEMS; i++) {
            int row_y = start_y + i * 9;
            bool is_selected = (i == s_cursor);

            uint32_t text_color = is_selected ? 0xFF00FFFF : 0xFFE0E0E0;
            char cursor_sym = is_selected ? '>' : ' ';

            char line_buf[64];
            if (i == 0) {
                snprintf(line_buf, sizeof(line_buf), "%c %-20s", cursor_sym, s_main_items[i].label);
                draw_text(fb, box_x + 8, row_y, line_buf, text_color);
                char diff_buf[20];
                snprintf(diff_buf, sizeof(diff_buf), "< LEVEL %d >", io_get_difficulty());
                draw_text(fb, box_x + 192, row_y, diff_buf, is_selected ? 0xFFFFD700 : 0xFF00FFCC);
            } else if (i == 1) {
                snprintf(line_buf, sizeof(line_buf), "%c %-20s", cursor_sym, s_main_items[i].label);
                draw_text(fb, box_x + 8, row_y, line_buf, text_color);
                bool blood = io_get_blood();
                const char *badge = blood ? "[RED ON]" : "[OFF   ]";
                uint32_t badge_color = blood ? 0xFFFF4444 : 0xFF888888;
                draw_text(fb, box_x + 224, row_y, badge, badge_color);
            } else if (i == 2) {
                snprintf(line_buf, sizeof(line_buf), "%c %-20s", cursor_sym, s_main_items[i].label);
                draw_text(fb, box_x + 8, row_y, line_buf, text_color);
                bool dup = io_get_allow_duplicates();
                const char *badge = dup ? "[ALLOWED]" : "[OFF    ]";
                uint32_t badge_color = dup ? 0xFF00FF66 : 0xFF888888;
                draw_text(fb, box_x + 216, row_y, badge, badge_color);
            } else if (i == 3) {
                snprintf(line_buf, sizeof(line_buf), "%c %-20s", cursor_sym, s_main_items[i].label);
                draw_text(fb, box_x + 8, row_y, line_buf, text_color);
                bool rnd = io_get_random_select();
                const char *badge = rnd ? "[ACTIVE ]" : "[OFF    ]";
                uint32_t badge_color = rnd ? 0xFFFFD700 : 0xFF888888;
                draw_text(fb, box_x + 216, row_y, badge, badge_color);
            } else if (i == 4) {
                snprintf(line_buf, sizeof(line_buf), "%c %-18s", cursor_sym, s_main_items[i].label);
                draw_text(fb, box_x + 8, row_y, line_buf, text_color);
                int layout = window_get_pad_layout();
                const char *layout_str = (layout == 0) ? "<SNK TYPE >" :
                                         (layout == 1) ? "<MODERN FG>" : "<KOF LINE >";
                draw_text(fb, box_x + 200, row_y, layout_str, is_selected ? 0xFFFFD700 : 0xFF33CCFF);
            } else if (i == 5) {
                snprintf(line_buf, sizeof(line_buf), "%c %-18s", cursor_sym, s_main_items[i].label);
                draw_text(fb, box_x + 8, row_y, line_buf, text_color);
                int filter = window_get_video_filter();
                const char *filter_str = (filter == 0) ? "<RAW NEAREST>" :
                                         (filter == 1) ? "<BILINEAR   >" :
                                         (filter == 2) ? "<CRT SCAN 50%>" :
                                         (filter == 3) ? "<CRT SCAN 75%>" :
                                         (filter == 4) ? "<TRINITRON   >" :
                                         (filter == 5) ? "<SCALE2X HD  >" :
                                                         "<CRT WARM CUR>";
                draw_text(fb, box_x + 184, row_y, filter_str, is_selected ? 0xFFFFD700 : 0xFF33CCFF);
            } else if (i == 6) {
                snprintf(line_buf, sizeof(line_buf), "%c %-18s", cursor_sym, s_main_items[i].label);
                draw_text(fb, box_x + 8, row_y, line_buf, text_color);
                int vsync = window_get_vsync();
                const char *vsync_str = vsync ? "< ON (SMOOTH) >" : "< OFF (LOW LAG)>";
                uint32_t vsync_color = vsync ? 0xFF00FF66 : 0xFFFFCC00;
                draw_text(fb, box_x + 184, row_y, vsync_str, is_selected ? 0xFFFFD700 : vsync_color);
            } else if (i == 7) {
                snprintf(line_buf, sizeof(line_buf), "%c %-27s", cursor_sym, s_main_items[i].label);
                draw_text(fb, box_x + 8, row_y, line_buf, is_selected ? 0xFFFFD700 : 0xFFFFAA33);
            } else if (i >= 8 && i <= 15) {
                int bit = i - 8;
                bool on = (dips >> bit) & 1;
                snprintf(line_buf, sizeof(line_buf), "%c %-27s", cursor_sym, s_main_items[i].label);
                draw_text(fb, box_x + 8, row_y, line_buf, text_color);

                const char *badge = on ? "[ON ]" : "[OFF]";
                uint32_t badge_color = on ? 0xFF00FF66 : 0xFF888888;
                draw_text(fb, box_x + 248, row_y, badge, badge_color);
            } else {
                snprintf(line_buf, sizeof(line_buf), "%c %s", cursor_sym, s_main_items[i].label);
                draw_text(fb, box_x + 8, row_y, line_buf, text_color);
            }
        }

        // Divider before description
        for (int x = box_x + 6; x < box_x + box_w - 6; x++) {
            fb[(box_y + 175) * NG_SCREEN_W + x] = 0xFF556677;
        }

        // Description text for selected option
        draw_text(fb, box_x + 10, box_y + 180, s_main_items[s_cursor].desc, 0xFFFFFFAA);

        // Footer Help text
        draw_text(fb, box_x + 14, box_y + 197, "[U/D]Move [L/R]Chg [RET]Select [TAB]Exit", 0xFF8899AA);

    } else if (s_page == PAGE_KEYBOARD) {
        // Keyboard Configuration Submenu
        draw_text(fb, box_x + 60, box_y + 6, "KEYBOARD KEY REMAPPING", 0xFFFFD700);

        // Divider line
        for (int x = box_x + 6; x < box_x + box_w - 6; x++) {
            fb[(box_y + 17) * NG_SCREEN_W + x] = 0xFF556677;
        }

        // Two columns of keys:
        // Column 1 (P1 keys): actions 0 to 9
        // Column 2 (P2 keys): actions 10 to 19
        draw_text(fb, box_x + 12, box_y + 22, "--- PLAYER 1 ---", 0xFF00FFCC);
        draw_text(fb, box_x + 160, box_y + 22, "--- PLAYER 2 ---", 0xFF00FFCC);

        for (int i = 0; i < 10; i++) {
            // Player 1 Entry
            int act1 = i;
            bool sel1 = (s_key_cursor == act1);
            char cur1 = sel1 ? '>' : ' ';
            uint32_t col1 = sel1 ? 0xFF00FFFF : 0xFFE0E0E0;
            if (sel1 && s_waiting_key) col1 = 0xFFFF4444;

            char buf1[64];
            snprintf(buf1, sizeof(buf1), "%c%-10s [%s]", cur1, window_get_key_action_name((key_action_t)act1),
                     (sel1 && s_waiting_key) ? "???" : window_get_key_name(window_get_key_mapping((key_action_t)act1)));
            draw_text(fb, box_x + 8, box_y + 35 + i * 11, buf1, col1);

            // Player 2 Entry
            int act2 = i + 10;
            bool sel2 = (s_key_cursor == act2);
            char cur2 = sel2 ? '>' : ' ';
            uint32_t col2 = sel2 ? 0xFF00FFFF : 0xFFE0E0E0;
            if (sel2 && s_waiting_key) col2 = 0xFFFF4444;

            char buf2[64];
            snprintf(buf2, sizeof(buf2), "%c%-10s [%s]", cur2, window_get_key_action_name((key_action_t)act2),
                     (sel2 && s_waiting_key) ? "???" : window_get_key_name(window_get_key_mapping((key_action_t)act2)));
            draw_text(fb, box_x + 156, box_y + 35 + i * 11, buf2, col2);
        }

        // Divider line above controls
        for (int x = box_x + 6; x < box_x + box_w - 6; x++) {
            fb[(box_y + 150) * NG_SCREEN_W + x] = 0xFF556677;
        }

        // Controls
        bool sel_rst = (s_key_cursor == KEY_MENU_RESET);
        char buf_rst[64];
        snprintf(buf_rst, sizeof(buf_rst), "%c [ Reset All Default Keys ]", sel_rst ? '>' : ' ');
        draw_text(fb, box_x + 20, box_y + 156, buf_rst, sel_rst ? 0xFFFFD700 : 0xFFFFAA33);

        bool sel_bak = (s_key_cursor == KEY_MENU_BACK);
        char buf_bak[64];
        snprintf(buf_bak, sizeof(buf_bak), "%c [ < Back to Main Menu ]", sel_bak ? '>' : ' ');
        draw_text(fb, box_x + 20, box_y + 168, buf_bak, sel_bak ? 0xFFFFD700 : 0xFF00FFCC);

        // Divider before footer
        for (int x = box_x + 6; x < box_x + box_w - 6; x++) {
            fb[(box_y + 182) * NG_SCREEN_W + x] = 0xFF556677;
        }

        if (s_waiting_key) {
            draw_text(fb, box_x + 20, box_y + 188, ">>> PRESS ANY KEY NOW (ESC TO CANCEL) <<<", 0xFFFF3333);
        } else {
            draw_text(fb, box_x + 16, box_y + 188, "Select an action and press ENTER to rebind.", 0xFFFFFFAA);
        }
        draw_text(fb, box_x + 14, box_y + 199, "[ENTER]Rebind  [ESC]Back to Main Menu", 0xFF8899AA);
    }
}
