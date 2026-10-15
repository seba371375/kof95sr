#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>

int main(void) {
    SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
    printf("Opening index 0 first time...\n");
    SDL_GameController *pad1 = SDL_GameControllerOpen(0);
    printf("pad1: %p\n", (void*)pad1);

    printf("Opening index 0 second time...\n");
    SDL_GameController *pad2 = SDL_GameControllerOpen(0);
    printf("pad2: %p\n", (void*)pad2);

    if (pad1) SDL_GameControllerClose(pad1);
    if (pad2) SDL_GameControllerClose(pad2);
    SDL_Quit();
    return 0;
}
