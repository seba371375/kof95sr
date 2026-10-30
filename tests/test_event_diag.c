#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>

int main(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("Init num joysticks: %d\n", SDL_NumJoysticks());
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        printf("Initial joystick index %d: %s (IsGameController: %d)\n",
               i, SDL_JoystickNameForIndex(i), SDL_IsGameController(i));
    }

    // Pump events
    SDL_PumpEvents();
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_CONTROLLERDEVICEADDED) {
            printf("SDL_CONTROLLERDEVICEADDED which=%d\n", e.cdevice.which);
        } else if (e.type == SDL_JOYDEVICEADDED) {
            printf("SDL_JOYDEVICEADDED which=%d\n", e.jdevice.which);
        }
    }

    SDL_Quit();
    return 0;
}
