#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>
#include <stdbool.h>

static SDL_GameController *s_pads[2] = {NULL, NULL};
static SDL_JoystickID      s_pad_ids[2] = {-1, -1};

static void add_controller(int device_index) {
    if (!SDL_IsGameController(device_index)) return;
    for (int p = 0; p < 2; p++) {
        if (!s_pads[p]) {
            s_pads[p] = SDL_GameControllerOpen(device_index);
            if (s_pads[p]) {
                SDL_Joystick *joy = SDL_GameControllerGetJoystick(s_pads[p]);
                s_pad_ids[p] = SDL_JoystickInstanceID(joy);
                const char *name = SDL_GameControllerName(s_pads[p]);
                printf("[input] Player %d gamepad connected: %s (ptr=%p, id=%d)\n",
                        p + 1, name ? name : "Generic Gamepad", (void*)s_pads[p], s_pad_ids[p]);
                return;
            }
        }
    }
}

int main(void) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
    SDL_Window *win = SDL_CreateWindow("Test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 320, 224, 0);

    // Initial loop in window_open
    int num_joysticks = SDL_NumJoysticks();
    printf("window_open: num_joysticks = %d\n", num_joysticks);
    for (int i = 0; i < num_joysticks; i++) {
        add_controller(i);
    }

    // Now poll events
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_CONTROLLERDEVICEADDED) {
            printf("SDL_CONTROLLERDEVICEADDED which = %d\n", e.cdevice.which);
            add_controller(e.cdevice.which);
        }
    }

    printf("Final state: s_pads[0]=%p, s_pads[1]=%p\n", (void*)s_pads[0], (void*)s_pads[1]);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
