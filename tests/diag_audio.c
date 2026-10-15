#include "ng_internal.h"
#include "romload.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const ng_game_t k_kof95;

int main(void) {
    size_t sz, m1_sz, sm1_sz;
    uint8_t *m1 = ng_read_file("roms", "084-m1.bin", &m1_sz);
    uint8_t *sm1 = ng_read_file("roms", "sm1.sm1", &sm1_sz);
    if (!m1 || !sm1) {
        fprintf(stderr, "Error reading m1 or sm1\n");
        return 1;
    }

    size_t vrom_total = 0x1000000;
    uint8_t *vrom_buf = (uint8_t *)calloc(1, vrom_total);
    size_t vsz = 0;
    uint8_t *v1 = ng_read_file("roms", "084-v1.bin", &vsz);
    if (v1) { memcpy(vrom_buf, v1, vsz); free(v1); }
    uint8_t *v2 = ng_read_file("roms", "084-v2.bin", &vsz);
    if (v2) { memcpy(vrom_buf + 0x400000, v2, vsz); free(v2); }
    uint8_t *v3 = ng_read_file("roms", "084-v3.bin", &vsz);
    if (v3) { memcpy(vrom_buf + 0x800000, v3, vsz); free(v3); }

    audio_init(m1, m1_sz, sm1, vrom_buf, vrom_total);
    audio_select_bios_rom(0); // Cart M1

    // Step audio and send coin / start / title command (e.g. 0x02 or 0x20)
    audio_command(0x02);

    int16_t buffer[800 * 2];
    int total_non_zero = 0;
    int max_amp = 0;

    // Simulate 300 frames (~5 seconds of game)
    for (int frame = 0; frame < 300; frame++) {
        extern int64_t ng_cycles;
        ng_cycles += 400000; // ~1 frame of 68k cycles (24MHz / 60)
        audio_take_samples(buffer, 800);
        for (int i = 0; i < 800 * 2; i++) {
            if (buffer[i] != 0) total_non_zero++;
            if (abs(buffer[i]) > max_amp) max_amp = abs(buffer[i]);
        }
    }

    printf("[diag_audio] 300 frames: non-zero samples = %d, max amplitude = %d\n",
           total_non_zero, max_amp);

    free(vrom_buf);
    free(m1);
    free(sm1);
    return 0;
}
