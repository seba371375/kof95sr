/*
 * audio.c — the Z80 sound CPU, its latches to the 68000, and the YM2610.
 *
 * Z80 memory: $0000-$7FFF fixed M1 ROM (or the BIOS SM1 while the system
 * latch selects the board ROMs), $8000/$C000/$E000/$F000 four switchable
 * windows of 16/8/4/2 KB, $F800-$FFFF 2 KB RAM. Reading ports $08-$0B
 * selects a window's bank from the upper byte of the port address.
 *
 * Ports: $00 (r) command from the 68k, $04-$07 YM2610, $08/$18 (w) NMI
 * enable/disable, $0C (w) reply to the 68k.
 *
 * A 68k command write latches the byte and raises NMI (when enabled); that
 * is how the BIOS and every sound driver talk. The Z80 runs lazily: it is
 * caught up to the 68k's clock (12 MHz / 3) whenever the 68k touches the
 * sound latches, and at the end of each frame.
 *
 * The YM2610 here models the timers, status and IRQ line that drivers
 * sync to. Tone generation is not implemented yet (ROADMAP.md).
 */
#include "ng_internal.h"
#include <neogeorecomp/cpu.h>
#include "z80.h"
#include "ym2610_bridge.h"
#include <string.h>
#include <stdlib.h>

#ifdef NG_HAVE_SDL
#include <SDL_mutex.h>
static SDL_mutex *s_audio_mutex = NULL;
#define AUDIO_LOCK()   do { if (s_audio_mutex) SDL_LockMutex(s_audio_mutex); } while(0)
#define AUDIO_UNLOCK() do { if (s_audio_mutex) SDL_UnlockMutex(s_audio_mutex); } while(0)
#else
#define AUDIO_LOCK()   do {} while(0)
#define AUDIO_UNLOCK() do {} while(0)
#endif

static z80 s_z;
static const uint8_t *s_m1, *s_sm1;
static size_t s_m1_size;
static int s_use_bios_rom = 1;
static const uint8_t *s_active_rom = NULL;
static size_t s_rom_mask = 0x1FFFF;
static uint8_t s_ram[0x800];
static uint32_t s_bank[4];          /* byte offsets for $F000, $E000, $C000, $8000 */
static uint8_t s_cmd, s_reply;
static int s_nmi_enabled, s_nmi_pending, s_nmi_line;
static int64_t s_z80_clock;         /* Z80 clocks executed, tracks ng_cycles / 3 */

/* ---- YM2610: backed by ymfm bridge ---- */
static void ym_update_timers(void) {
    ym2610_bridge_clock(s_z80_clock);
}

static void ym_write(int port, uint8_t v) {
    ym2610_bridge_write((uint8_t)port, v);
}

static uint8_t ym_read(int port) {
    return ym2610_bridge_read((uint8_t)port);
}

/* ---- Z80 bus ---- */

static inline void update_active_rom(void) {
    s_active_rom = s_use_bios_rom ? s_sm1 : s_m1;
    size_t size = s_use_bios_rom ? 0x20000 : s_m1_size;
    s_rom_mask = size ? (size - 1) : 0x1FFFF;
}

static uint8_t z_read(void *u, uint16_t a) {
    (void)u;
    if (a < 0x8000) return s_active_rom ? s_active_rom[a & s_rom_mask] : 0xFF;
    if (a >= 0xF800) return s_ram[a & 0x7FF];
    uint32_t off;
    if (a >= 0xF000) off = s_bank[0] + (a & 0x7FF);
    else if (a >= 0xE000) off = s_bank[1] + (a & 0xFFF);
    else if (a >= 0xC000) off = s_bank[2] + (a & 0x1FFF);
    else off = s_bank[3] + (a & 0x3FFF);
    return s_active_rom ? s_active_rom[off & s_rom_mask] : 0xFF;
}

static void z_write(void *u, uint16_t a, uint8_t v) {
    (void)u;
    if (a >= 0xF800) s_ram[a & 0x7FF] = v;
}

/* NMI is edge-triggered: one NMI per rise of (enabled && command pending). */
static void update_nmi(void) {
    int line = s_nmi_enabled && s_nmi_pending;
    if (line && !s_nmi_line) z80_gen_nmi(&s_z);
    s_nmi_line = line;
}

/* The core passes only the low port byte; bank selects need the high byte,
 * which is A for IN A,(n) and B for IN r,(C). */
static uint8_t port_high(z80 *z) {
    return z_read(NULL, (uint16_t)(z->pc - 2)) == 0xDB ? z->a : z->b;
}

static uint8_t z_in(z80 *z, uint8_t port) {
    switch (port & 0x0F) {
    case 0x00:
        s_nmi_pending = 0;
        update_nmi();
        return s_cmd;
    case 0x04: case 0x05: case 0x06: case 0x07:
        ym_update_timers();
        return ym_read(port & 3);
    case 0x08: case 0x09: case 0x0A: case 0x0B: {
        static const uint32_t unit[4] = {0x800, 0x1000, 0x2000, 0x4000};
        int w = port & 3;
        s_bank[w] = port_high(z) * unit[w];
        return 0;
    }
    default: return 0;
    }
}

static void z_out(z80 *z, uint8_t port, uint8_t v) {
    (void)z;
    switch (port & 0x0F) {
    case 0x00: s_cmd = 0; break;
    case 0x04: case 0x05: case 0x06: case 0x07:
        ym_update_timers();
        ym_write(port & 3, v);
        break;
    case 0x08: s_nmi_enabled = !(port & 0x10); update_nmi(); break;
    case 0x0C: s_reply = v; break;
    default: break;
    }
}

/* ---- interface to the 68k side ---- */

void audio_reset(void) {
    z80_init(&s_z);
    s_z.read_byte = z_read;
    s_z.write_byte = z_write;
    s_z.port_in = z_in;
    s_z.port_out = z_out;
    memset(s_ram, 0, sizeof(s_ram));
    s_bank[0] = 0x1E * 0x800; s_bank[1] = 0x0E * 0x1000;
    s_bank[2] = 0x06 * 0x2000; s_bank[3] = 0x02 * 0x4000;
    s_nmi_enabled = 0; s_nmi_pending = 0; s_nmi_line = 0;
    ym2610_bridge_reset();
    s_z80_clock = ng_cycles / 3;
    s_z.cyc = 0;
}

int audio_init(const uint8_t *m1, size_t m1_size, const uint8_t *sm1,
               const uint8_t *vrom, size_t vrom_size) {
#ifdef NG_HAVE_SDL
    if (!s_audio_mutex) s_audio_mutex = SDL_CreateMutex();
#endif
    s_m1 = m1; s_m1_size = m1_size; s_sm1 = sm1;
    s_cmd = 0; s_reply = 0;
    update_active_rom();
    ym2610_bridge_init(vrom, vrom_size);
    audio_reset();
    return 0;
}

static void audio_sync_unlocked(void) {
    int64_t target = ng_cycles / 3;
    while (s_z80_clock < target) {
        unsigned long before = s_z.cyc;
        s_z.int_pending = ym2610_bridge_irq_pending();
        z80_step(&s_z);
        s_z80_clock += (int64_t)(s_z.cyc - before);
        ym_update_timers();
    }
}

void audio_sync(void) {
    AUDIO_LOCK();
    audio_sync_unlocked();
    AUDIO_UNLOCK();
}

void audio_command(uint8_t cmd) {
    AUDIO_LOCK();
    audio_sync_unlocked();
    io_on_audio_command(cmd);
    s_cmd = cmd;
    s_nmi_pending = 1;
    update_nmi();
    AUDIO_UNLOCK();
}

uint8_t audio_reply(void) {
    AUDIO_LOCK();
    audio_sync_unlocked();
    uint8_t r = s_reply;
    AUDIO_UNLOCK();
    return r;
}

void audio_select_bios_rom(int bios) {
    if (bios == s_use_bios_rom) return;
    AUDIO_LOCK();
    audio_sync_unlocked();
    s_use_bios_rom = bios;
    update_active_rom();
    /* Swapping the Z80's ROM restarts it, so the newly selected driver
     * boots from its own reset vector. */
    uint8_t cmd = s_cmd, reply = s_reply;
    audio_reset();
    s_cmd = cmd; s_reply = reply;
    AUDIO_UNLOCK();
}

size_t audio_take_samples(int16_t *out, size_t max_frames) {
    AUDIO_LOCK();
    audio_sync_unlocked();
    size_t gen = ym2610_bridge_generate(out, max_frames);
    AUDIO_UNLOCK();
    return gen;
}

void audio_shutdown(void) {
    ym2610_bridge_shutdown();
#ifdef NG_HAVE_SDL
    if (s_audio_mutex) {
        SDL_DestroyMutex(s_audio_mutex);
        s_audio_mutex = NULL;
    }
#endif
}
