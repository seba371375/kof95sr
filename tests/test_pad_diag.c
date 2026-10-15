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
        printf("[%d] Name: %s\n", i, SDL_JoystickNameForIndex(i));
        printf("    IsGameController: %d\n", SDL_IsGameController(i));
        SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(i);
        char guid_str[64];
        SDL_JoystickGetGUIDString(guid, guid_str, sizeof(guid_str));
        printf("    GUID: %s\n", guid_str);
        printf("    Device Instance: %d\n", SDL_JoystickGetDeviceInstanceID(i));
        if (SDL_IsGameController(i)) {
            SDL_GameController *pad = SDL_GameControllerOpen(i);
            if (pad) {
                SDL_Joystick *j = SDL_GameControllerGetJoystick(pad);
                printf("    Opened as Controller! InstanceID: %d\n", SDL_JoystickInstanceID(j));
                printf("    Mapping: %s\n", SDL_GameControllerMapping(pad));
                SDL_GameControllerClose(pad);
            } else {
                printf("    Failed to open as GameController: %s\n", SDL_GetError());
            }
        }
    }

    SDL_Quit();
    return 0;
}
