#include "ng_internal.h"
#include "romload.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const ng_game_t k_kof95;
extern const ng_func_entry_t k_kof95_funcs[];
extern const size_t k_kof95_nfuncs;

int main(void) {
    size_t sz, bios_sz, sfix_sz, sm1_sz, lo_sz, srom_sz, m1_sz;
    uint8_t *prom = ng_load_prom("roms", &k_kof95);
    uint8_t *bios = ng_read_file("roms", "asia-s3.rom", &bios_sz);
    uint8_t *sfix = ng_read_file("roms", "sfix.sfix", &sfix_sz);
    uint8_t *sm1 = ng_read_file("roms", "sm1.sm1", &sm1_sz);
    uint8_t *lo = ng_read_file("roms", "000-lo.lo", &lo_sz);
    uint8_t *crom = ng_load_crom("roms", &k_kof95);
    uint8_t *srom = ng_read_file("roms", k_kof95.srom, &srom_sz);
    uint8_t *m1 = ng_read_file("roms", k_kof95.m1, &m1_sz);

    if (!prom || !bios || !sfix || !sm1 || !lo || !crom || !srom || !m1) {
        fprintf(stderr, "Missing ROMs!\n");
        return 1;
    }
    ng_swap16(bios, 0x20000);

    uint8_t *vrom_buf = (uint8_t *)calloc(1, 0x1000000);
    size_t vsz = 0;
    uint8_t *v1 = ng_read_file("roms", "084-v1.bin", &vsz);
    if (v1) { memcpy(vrom_buf, v1, vsz); free(v1); }
    uint8_t *v2 = ng_read_file("roms", "084-v2.bin", &vsz);
    if (v2) { memcpy(vrom_buf + 0x400000, v2, vsz); free(v2); }
    uint8_t *v3 = ng_read_file("roms", "084-v3.bin", &vsz);
    if (v3) { memcpy(vrom_buf + 0x800000, v3, vsz); free(v3); }

    ng_board_t board = {prom, k_kof95.prom_size, bios, 1};
    bus_init(&board);
    io_reset(1);
    video_load(crom, k_kof95.crom_size, srom, srom_sz, sfix, lo);
    audio_init(m1, m1_sz, sm1, vrom_buf, 0x1000000);
    exec_init(k_kof95_funcs, k_kof95_nfuncs, 0);

    // Power on sequence
    extern int64_t ng_cycles, ng_frame_start;
    extern int ng_irq_pending;
    ng_cycles = 0;
    ng_frame_start = 0;
    bus_reset();
    lspc_reset();
    pal_select_bank(0);
    video_set_fix_bios(1);
    audio_select_bios_rom(1);
    audio_reset();
    exec_reset();
    ng_irq_pending = 4;

    static uint32_t s_fb[NG_SCREEN_W * NG_SCREEN_H];
    static int16_t samples[2048 * 2];

    uint64_t total_samples = 0;
    uint64_t total_non_zero = 0;
    int max_amp = 0;

    printf("Running 1000 frames with input script...\n");
    // Load script
    load_script("tests/test_random_select_3x.txt");

    for (long frame = 0; frame < 1000; frame++) {
        ng_input_t in = {0};
        script_input(frame, &in);
        io_set_inputs(&in);
        io_sync_soft_dips();

        // Run frame
        int64_t fs = ng_frame_start;
        for (int line = 0; line < NG_LINES_PER_FRAME; line++) {
            int64_t ls = fs + (int64_t)line * NG_CLOCKS_PER_LINE;
            while (ng_cycles < ls) {
                int64_t target = ls, d = lspc_timer_deadline();
                if (d < target) target = d;
                if (target > ng_cycles) exec_run(target);
                if (ng_cycles >= lspc_timer_deadline()) lspc_timer_fire();
            }
            if (line == NG_VBLANK_LINE) {
                while (ng_cycles < ls + NG_VBLANK_HPOS * 2) {
                    int64_t target = ls + NG_VBLANK_HPOS * 2, d = lspc_timer_deadline();
                    if (d < target) target = d;
                    if (target > ng_cycles) exec_run(target);
                    if (ng_cycles >= lspc_timer_deadline()) lspc_timer_fire();
                }
                lspc_vblank();
            }
        }
        ng_frame_start = fs + NG_CLOCKS_PER_FRAME;
        while (ng_cycles < ng_frame_start) {
            int64_t target = ng_frame_start, d = lspc_timer_deadline();
            if (d < target) target = d;
            if (target > ng_cycles) exec_run(target);
            if (ng_cycles >= lspc_timer_deadline()) lspc_timer_fire();
        }
        audio_sync();

        static uint32_t s_audio_frac = 0;
        s_audio_frac += 48000000;
        size_t n_samples = s_audio_frac / 59185606;
        s_audio_frac %= 59185606;

        size_t taken = audio_take_samples(samples, n_samples);
        total_samples += taken;
        for (size_t i = 0; i < taken * 2; i++) {
            if (samples[i] != 0) total_non_zero++;
            if (abs(samples[i]) > max_amp) max_amp = abs(samples[i]);
        }

        if (frame % 200 == 0 || frame == 750 || frame == 800 || frame == 999) {
            printf("Frame %ld: non-zero samples = %llu / %llu (%.1f%%), max amplitude = %d\n",
                   frame, total_non_zero, total_samples * 2,
                   total_samples ? (100.0 * total_non_zero / (total_samples * 2)) : 0.0,
                   max_amp);
        }
    }

    return 0;
}
