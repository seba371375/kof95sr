#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>

int main(void) {
    if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    int n_joy = SDL_NumJoysticks();
    printf("Number of joysticks detected: %d\n", n_joy);
    for (int i = 0; i < n_joy; i++) {
        printf("Joystick %d: %s (IsGameController: %d)\n",
               i, SDL_JoystickNameForIndex(i), SDL_IsGameController(i));
    }

    SDL_Quit();
    return 0;
}
