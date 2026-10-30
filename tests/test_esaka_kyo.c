#include "ng_internal.h"
#include "romload.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const ng_game_t k_kof95;

static void write_wav_header(FILE *f, int num_samples, int sample_rate) {
    int data_size = num_samples * 2 * 2;
    int riff_size = data_size + 36;
    uint8_t hdr[44] = {
        'R','I','F','F',
        (uint8_t)(riff_size & 0xFF), (uint8_t)((riff_size >> 8) & 0xFF),
        (uint8_t)((riff_size >> 16) & 0xFF), (uint8_t)((riff_size >> 24) & 0xFF),
        'W','A','V','E',
        'f','m','t',' ',
        16, 0, 0, 0,
        1, 0, 2, 0,
        (uint8_t)(sample_rate & 0xFF), (uint8_t)((sample_rate >> 8) & 0xFF),
        (uint8_t)((sample_rate >> 16) & 0xFF), (uint8_t)((sample_rate >> 24) & 0xFF),
        (uint8_t)((sample_rate * 4) & 0xFF), (uint8_t)(((sample_rate * 4) >> 8) & 0xFF),
        (uint8_t)(((sample_rate * 4) >> 16) & 0xFF), (uint8_t)(((sample_rate * 4) >> 24) & 0xFF),
        4, 0, 16, 0,
        'd','a','t','a',
        (uint8_t)(data_size & 0xFF), (uint8_t)((data_size >> 8) & 0xFF),
        (uint8_t)((data_size >> 16) & 0xFF), (uint8_t)((data_size >> 24) & 0xFF)
    };
    fwrite(hdr, 1, 44, f);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("=== Test: Play 'FUNKY ESAKA' (Hero Team BGM) & Kyo Voices ===\n");

    size_t m1_sz, sm1_sz;
    uint8_t *m1 = ng_read_file("roms", k_kof95.m1, &m1_sz);
    uint8_t *sm1 = ng_read_file("roms", "sm1.sm1", &sm1_sz);
    if (!m1 || !sm1) {
        fprintf(stderr, "Error loading M1 / SM1 ROMs!\n");
        return 1;
    }

    size_t vrom_total = 0x1000000;
    uint8_t *vrom_buf = (uint8_t *)calloc(1, vrom_total);
    for (int nv = 0; nv < NG_MAX_PARTS && k_kof95.vrom[nv]; nv++) {
        size_t vsz = 0;
        uint8_t *v = ng_read_file("roms", k_kof95.vrom[nv], &vsz);
        if (v) {
            uint32_t dest = k_kof95.vrom_offsets[nv];
            memcpy(vrom_buf + dest, v, vsz);
            free(v);
        }
    }

    audio_init(m1, m1_sz, sm1, vrom_buf, vrom_total);

    // Boot Neo Geo with BIOS SM1 first, then switch to Cart M1 (exact hardware boot sequence)
    audio_select_bios_rom(1);
    extern int64_t ng_cycles;
    for (int f = 0; f < 30; f++) {
        ng_cycles += 405440;
        audio_sync();
    }

    printf("[1/3] Switching to Cart M1 ROM...\n");
    audio_select_bios_rom(0);

    // Let Cart Z80 sound driver execute its initialization loop and enable NMI
    for (int f = 0; f < 60; f++) {
        ng_cycles += 405440;
        audio_sync();
    }

    // Send driver reset/init command 0x01, then wait a few frames
    printf("[2/4] Sending Sound Driver Init Command 0x01...\n");
    audio_command(0x01);
    for (int f = 0; f < 30; f++) {
        ng_cycles += 405440;
        audio_sync();
    }

    // Now send command 0x22: FUNKY ESAKA (Hero Team: Kyo Kusanagi, Benimaru, Daimon)!
    printf("[3/4] Sending Sound Command 0x22: FUNKY ESAKA (Hero Team theme)...\n");
    audio_command(0x22);

    FILE *f_wav = fopen("esaka_kyo.wav", "wb");
    write_wav_header(f_wav, 0, 48000); // placeholder

    int16_t samples[1024 * 2];
    int total_samples = 0;

    // Record 6 seconds of ESAKA BGM (~360 frames)
    for (int f = 0; f < 360; f++) {
        ng_cycles += 405440;
        size_t n = audio_take_samples(samples, 811);
        fwrite(samples, 2, n * 2, f_wav);
        total_samples += (int)n;
    }

    // Now trigger Kyo voices / sound effects (e.g. 0x61, 0x1A, 0x38, etc.)
    printf("[3/3] Sending Sound Command 0x61 & 0x1A (Kyo SFX / Voices)...\n");
    audio_command(0x61);

    for (int f = 0; f < 60; f++) {
        ng_cycles += 405440;
        size_t n = audio_take_samples(samples, 811);
        fwrite(samples, 2, n * 2, f_wav);
        total_samples += (int)n;
    }

    audio_command(0x1A);

    for (int f = 0; f < 90; f++) {
        ng_cycles += 405440;
        size_t n = audio_take_samples(samples, 811);
        fwrite(samples, 2, n * 2, f_wav);
        total_samples += (int)n;
    }

    // Finalize WAV
    fseek(f_wav, 0, SEEK_SET);
    write_wav_header(f_wav, total_samples, 48000);
    fclose(f_wav);

    printf("Successfully generated esaka_kyo.wav (%d samples, %.1f seconds)!\n",
           total_samples, (double)total_samples / 48000.0);

    return 0;
}
