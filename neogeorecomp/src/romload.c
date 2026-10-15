/*
 * romload.c - reads a set's ROM files into the regions the board expects.
 * Shared by the runtime (neogeo.c) and the recompiler, so both see the
 * program exactly the same way.
 */
#include "romload.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint8_t *ng_read_file(const char *dir, const char *name, size_t *size) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    FILE *f = fopen(path, "rb");

    /* Fallback search directories (especially useful on PS5 homebrew payloads and relative runs) */
    if (!f) {
        static const char *fallback_dirs[] = {
            "/data/kof95/roms",
            "/data/kof95",
            "/user/homebrew/kof95/roms",
            "/user/homebrew/kof95",
            "roms",
            "."
        };
        for (size_t i = 0; i < sizeof(fallback_dirs) / sizeof(fallback_dirs[0]); i++) {
            if (dir && !strcmp(dir, fallback_dirs[i])) continue;
            snprintf(path, sizeof(path), "%s/%s", fallback_dirs[i], name);
            f = fopen(path, "rb");
            if (f) break;
        }
    }

    if (!f) { fprintf(stderr, "error: cannot open %s (checked '%s' and fallbacks)\n", name, dir ? dir : "."); return NULL; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = (uint8_t *)malloc((size_t)n);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(buf); return NULL; }
    fclose(f);
    *size = (size_t)n;
    return buf;
}

void ng_swap16(uint8_t *p, size_t n) {
    for (size_t i = 0; i + 1 < n; i += 2) { uint8_t t = p[i]; p[i] = p[i + 1]; p[i + 1] = t; }
}

uint8_t *ng_load_prom(const char *dir, const ng_game_t *g) {
    uint8_t *prom = (uint8_t *)calloc(1, g->prom_size);
    for (int i = 0; i < NG_MAX_PARTS && g->prom[i].file; i++) {
        const ng_rom_part_t *p = &g->prom[i];
        size_t size;
        uint8_t *data = ng_read_file(dir, p->file, &size);
        if (!data) { free(prom); return NULL; }
        if (p->file_offset + p->length > size || p->dest_offset + p->length > g->prom_size) {
            fprintf(stderr, "error: %s is smaller than the layout expects\n", p->file);
            free(data); free(prom); return NULL;
        }
        memcpy(prom + p->dest_offset, data + p->file_offset, p->length);
        if (p->swap) ng_swap16(prom + p->dest_offset, p->length);
        free(data);
    }
    return prom;
}

/* C ROMs come in pairs holding bitplanes 0-1 and 2-3; the renderer wants
 * them byte-interleaved (even byte from the first file). */
uint8_t *ng_load_crom(const char *dir, const ng_game_t *g) {
    uint8_t *crom = (uint8_t *)calloc(1, g->crom_size);
    int explicit_offsets = 0;
    for (int i = 0; i < NG_MAX_PARTS; i++)
        if (g->crom_offsets[i]) explicit_offsets = 1;
    size_t at = 0;
    for (int i = 0; i < NG_MAX_PARTS && g->crom_pairs[i][0]; i++) {
        size_t s0, s1;
        if (explicit_offsets) at = g->crom_offsets[i];
        uint8_t *a = ng_read_file(dir, g->crom_pairs[i][0], &s0);
        uint8_t *b = ng_read_file(dir, g->crom_pairs[i][1], &s1);
        if (!a || !b || s0 != s1 || at + s0 * 2 > g->crom_size) {
            if (a && b)
                fprintf(stderr, "error: C ROM pair %s/%s does not fit the layout\n",
                        g->crom_pairs[i][0], g->crom_pairs[i][1]);
            free(a); free(b); free(crom); return NULL;
        }
        for (size_t k = 0; k < s0; k++) { crom[at + 2 * k] = a[k]; crom[at + 2 * k + 1] = b[k]; }
        at += s0 * 2;
        free(a); free(b);
    }
    return crom;
}

