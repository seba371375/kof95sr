#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>

int main(void) {
    SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
    printf("Initial NumJoysticks: %d\n", SDL_NumJoysticks());
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        printf("Initial joystick index %d: is_gc=%d\n", i, SDL_IsGameController(i));
    }
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_CONTROLLERDEVICEADDED) {
            printf("SDL_CONTROLLERDEVICEADDED: which = %d\n", e.cdevice.which);
        }
    }
    SDL_Quit();
    return 0;
}
