extern "C" {
#include "ng_internal.h"
#include "romload.h"
#include "ym2610_bridge.h"
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

extern const ng_game_t k_kof95;

void write_wav(const char *filename, const std::vector<int16_t> &samples, int rate) {
    FILE *f = fopen(filename, "wb");
    if (!f) return;
    int data_size = samples.size() * 2;
    int chunk_size = 36 + data_size;
    int subchunk1_size = 16;
    short audio_format = 1;
    short num_channels = 2;
    int sample_rate = rate;
    int byte_rate = rate * 2 * 2;
    short block_align = 4;
    short bits_per_sample = 16;

    fwrite("RIFF", 1, 4, f);
    fwrite(&chunk_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    fwrite(&subchunk1_size, 4, 1, f);
    fwrite(&audio_format, 2, 1, f);
    fwrite(&num_channels, 2, 1, f);
    fwrite(&sample_rate, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bits_per_sample, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&data_size, 4, 1, f);
    fwrite(samples.data(), 2, samples.size(), f);
    fclose(f);
    printf("Wrote %s (%zu samples, %d Hz)\n", filename, samples.size() / 2, rate);
}

int main(void) {
    size_t sz, m1_sz, sm1_sz;
    uint8_t *m1 = ng_read_file("roms", k_kof95.m1, &m1_sz);
    uint8_t *sm1 = ng_read_file("roms", "sm1.sm1", &sm1_sz);
    if (!m1 || !sm1) {
        printf("Error reading m1/sm1\n");
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
    audio_select_bios_rom(0); // Cart M1

    // Step audio and send command 0x22 (ESAKA BGM in KOF95!)
    // Let's verify BGM commands:
    // In KOF95:
    // 0x22 = FUNKY ESAKA (Hero Team theme)
    // 0x23 = CLUB-M (AOF Team)
    // 0x24 = Ryuuko no Ken
    // 0x2C = Character Select
    printf("Sending Sound Command 0x22 (FUNKY ESAKA - Japan Team)...\n");
    audio_command(0x22);

    std::vector<int16_t> recorded;
    int16_t buf[1024 * 2];

    extern int64_t ng_cycles;
    for (int frame = 0; frame < 300; frame++) {
        ng_cycles += 405440; // 24MHz / 59.1856Hz ≈ 405440 68k clocks per frame
        size_t n = audio_take_samples(buf, 811);
        for (size_t i = 0; i < n * 2; i++) {
            recorded.push_back(buf[i]);
        }
    }

    // Now send Kyo voice (e.g. 0x38 or 0x61 or Kyo attack SFX)
    printf("Sending Sound Command 0x61 (Kyo SFX / Voice)...\n");
    audio_command(0x61);

    for (int frame = 0; frame < 200; frame++) {
        ng_cycles += 405440;
        size_t n = audio_take_samples(buf, 811);
        for (size_t i = 0; i < n * 2; i++) {
            recorded.push_back(buf[i]);
        }
    }

    write_wav("test_audio.wav", recorded, 48000);

    return 0;
}
