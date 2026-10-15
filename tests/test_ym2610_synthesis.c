#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>

#include "ng_internal.h"
#include "ym2610_bridge.h"

int main(void) {
    printf("[test_sound] Initializing test...\n");

    // Allocate dummy V ROM buffer
    size_t vrom_sz = 0x1000000;
    uint8_t *vrom = (uint8_t *)calloc(1, vrom_sz);
    assert(vrom != NULL);

    ym2610_bridge_init(vrom, vrom_sz);

    // Generate silence before any sound commands
    int16_t buffer[1024 * 2];
    ym2610_bridge_generate(buffer, 1024);

    // Write SSG tone on Channel A:
    // Port 0: reg 0 (fine tune) = 0x50
    ym2610_bridge_write(0, 0x00);
    ym2610_bridge_write(1, 0x50);
    // Port 0: reg 1 (coarse tune) = 0x00
    ym2610_bridge_write(0, 0x01);
    ym2610_bridge_write(1, 0x00);
    // Port 0: reg 7 (mixer control) = enable tone A (bit 0 = 0)
    ym2610_bridge_write(0, 0x07);
    ym2610_bridge_write(1, 0x3E);
    // Port 0: reg 8 (amplitude A) = 0x0F (max volume)
    ym2610_bridge_write(0, 0x08);
    ym2610_bridge_write(1, 0x0F);

    // Generate samples with active SSG tone
    ym2610_bridge_generate(buffer, 1024);

    int non_zero = 0;
    int max_val = 0;
    for (int i = 0; i < 1024 * 2; i++) {
        if (buffer[i] != 0) non_zero++;
        if (abs(buffer[i]) > max_val) max_val = abs(buffer[i]);
    }

    printf("[test_sound] Generated 1024 frames: non-zero samples = %d, max amplitude = %d\n", non_zero, max_val);
    assert(non_zero > 0);
    assert(max_val > 0);

    ym2610_bridge_shutdown();
    free(vrom);
    printf("[test_sound] SUCCESS: YM2610 synthesis verified!\n");
    return 0;
}
