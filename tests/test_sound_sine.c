#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>
#include <math.h>

int main(void) {
    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof(want));
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!dev) {
        printf("Failed to open audio: %s\n", SDL_GetError());
        return 1;
    }

    printf("Audio device opened: %s, freq=%d, ch=%d\n", SDL_GetCurrentAudioDriver(), have.freq, have.channels);
    SDL_PauseAudioDevice(dev, 0);

    // Generate 1 second of 440 Hz beep
    int16_t buf[48000 * 2];
    for (int i = 0; i < 48000; i++) {
        int16_t sample = (int16_t)(sin(2.0 * M_PI * 440.0 * i / 48000.0) * 16000.0);
        buf[i * 2 + 0] = sample;
        buf[i * 2 + 1] = sample;
    }

    SDL_QueueAudio(dev, buf, sizeof(buf));
    printf("Playing 1s beep...\n");
    SDL_Delay(1200);

    SDL_CloseAudioDevice(dev);
    SDL_Quit();
    return 0;
}
