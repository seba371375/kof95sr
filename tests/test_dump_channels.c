#include "ng_internal.h"
#include "romload.h"
#include "ym2610_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const ng_game_t k_kof95;

int main(void) {
    size_t sz, m1_sz, sm1_sz;
    uint8_t *m1 = ng_read_file("roms", k_kof95.m1, &m1_sz);
    uint8_t *sm1 = ng_read_file("roms", "sm1.sm1", &sm1_sz);
    if (!m1 || !sm1) return 1;

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
    audio_select_bios_rom(0);

    // Run 100 frames of dummy Z80 execution
    extern int64_t ng_cycles;
    for (int f = 0; f < 100; f++) {
        ng_cycles += 405440;
        audio_sync();
    }

    int16_t out[811 * 2];
    audio_take_samples(out, 811);

    printf("Samples generated after 100 frames:\n");
    for (int i = 0; i < 20; i++) {
        printf("sample[%d] L=%d, R=%d\n", i, out[i*2], out[i*2+1]);
    }

    return 0;
}
