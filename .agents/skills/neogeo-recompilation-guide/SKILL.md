---
name: neogeo-recompilation-guide
description: >-
  Expert guide, architectural patterns, and troubleshooting manual for static recompilation
  of Neo Geo MVS/AES arcade titles to modern C/C++ platforms (macOS, Linux, Windows, PS5).
  Use when developing, debugging, porting, or adding features to Neo Geo static recompiled games.
---

# Neo Geo Static Recompilation Guide (`neogeo-recomp`)

This skill captures the end-to-end architecture, technical discoveries, and proven patterns established during the development and enhancement of the **King of Fighters '95** static recompilation project.

Use this runbook whenever porting a new Neo Geo game, recompiling M68000 routines, fixing audio/video pacing bugs, modifying memory/WRAM, or hooking gameplay logic.

---

## 🏛️ System Architecture Overview

A Neo Geo static recompilation project consists of three distinct layers:

```
┌─────────────────────────────────────────────────────────────┐
│ 1. Host Application (e.g. kof95)                            │
│    - Game ROM mapping (P, S, M, V, C ROM offsets & bank sizes)│
│    - Recompiled C blocks generated AOT by m68krecomp       │
│    - Custom hooks, character logic, DIP switch sync        │
├─────────────────────────────────────────────────────────────┤
│ 2. Neo Geo Runtime Engine (neogeorecomp)                    │
│    - Bus & Memory Management (M68000 WRAM, Z80 RAM, BIOS)   │
│    - LSPC Video Renderer (scanline raster, VRAM, auto-anim) │
│    - Audio Subsystem (Z80 CPU + YM2610 FM/SSG/ADPCM via ymfm)│
│    - OSD Overlay & Configuration System (320x224 soft-draw) │
├─────────────────────────────────────────────────────────────┤
│ 3. Platform & Windowing (window_sdl / platform)             │
│    - SDL2 / OpenGL / Vulkan backends                        │
│    - Gamepad input deduplication & customizable layouts     │
│    - Frame pacing to arcade refresh rate (59.1856 Hz)        │
└─────────────────────────────────────────────────────────────┘
```

---

## 🔊 Audio Subsystem: YM2610 & Frame Pacing

Neo Geo sound is driven by a secondary **Z80 CPU (3.579545 MHz)** communicating with a **Yamaha YM2610 (OPNB)** sound chip.

### 1. Clock Mismatch & Fidelity (`ymfm`)
The YM2610 has an internal sampling frequency of **55,555 Hz** (FM: 55.5 kHz, SSG: 222 kHz, ADPCM-A: 18.5 kHz, ADPCM-B: up to 55.5 kHz).
- **Pitfall:** Using standard `set_fidelity(OPN_FIDELITY_MAX)` in `ymfm` creates a 9x clock multiplier mismatch that causes sample repetition, pitch distortion, and severe buffer underruns.
- **Solution:** Always set low/native fidelity:
  ```cpp
  s_ym2610->set_fidelity(ymfm::OPN_FIDELITY_MIN);
  ```

### 2. Audio Pacing Formula
The Neo Geo MVS master clock runs at **59.185606 Hz**. To stream 48 kHz stereo audio smoothly without glitches:
```c
// In frame loop (neogeo.c):
static uint64_t s_audio_frac = 0;
s_audio_frac += (uint64_t)48000 * 1000000;
size_t n_samples = (size_t)(s_audio_frac / 59185606);
s_audio_frac %= 59185606;
size_t taken = audio_take_samples(samples, n_samples);
platform_audio(samples, taken);
```

### 3. Attract Sound & Soft-DIP Unmuting
Many MVS titles boot with Demo Sound muted by default:
- Check Backup RAM offset `$D00046` (MVS Cabinet setting: `0 = Demo Sound ON`).
- Check Soft-DIP `$10FD8C` (Game setting: `0 = Demo Sound ON`).

---

## 🎮 Gamepad Input & Dual-Controller Deduplication

Modern OSes (macOS, Windows, Linux) often register physical controllers as two separate devices (e.g. DualSense registers a Gamepad device and a Gyro/Touchpad HID device).

### The Duplicate Player Bug
Opening controllers sequentially with `SDL_GameControllerOpen(device_index)` will assign Device 0 to Player 1 and Device 1 to Player 2, causing a **single controller to control both players**.

### The Solution Pattern
Track physical device instance IDs before opening:
```c
static void add_controller(int device_index) {
    if (!SDL_IsGameController(device_index)) return;

    SDL_JoystickID jid = SDL_JoystickGetDeviceInstanceID(device_index);
    for (int p = 0; p < 2; p++) {
        if (s_pads[p] && s_pad_ids[p] == jid) {
            return; // Already open in a slot
        }
    }

    for (int p = 0; p < 2; p++) {
        if (!s_pads[p]) {
            s_pads[p] = SDL_GameControllerOpen(device_index);
            if (s_pads[p]) {
                SDL_Joystick *joy = SDL_GameControllerGetJoystick(s_pads[p]);
                s_pad_ids[p] = SDL_JoystickInstanceID(joy);
                return;
            }
        }
    }
}
```

---

## 📺 OSD Menu & Software Overlay Rendering

To make on-screen configuration (difficulty, cheats, controller mapping) work across all graphics APIs (SDL Accelerated, OpenGL, Vulkan, PS5):

1. **Draw directly to the 320x224 ARGB Framebuffer:**
   - Use an 8x8 font table (`osd_font.h`).
   - Software Alpha Blending helper:
     ```c
     static inline uint32_t blend_pixel(uint32_t bg, uint32_t fg, uint8_t a) {
         uint32_t rb  = bg & 0x00FF00FF;
         uint32_t g   = bg & 0x0000FF00;
         uint32_t frb = fg & 0x00FF00FF;
         uint32_t fg_ = fg & 0x0000FF00;
         rb += ((frb - rb) * a) >> 8;
         g  += ((fg_ - g) * a) >> 8;
         return 0xFF000000 | (rb & 0x00FF00FF) | (g & 0x0000FF00);
     }
     ```
2. **Suppress Input during OSD:** When `osd_menu_is_open() == true`, clear input state (`memset(in, 0, sizeof(*in))`) to prevent player actions while navigating options.

---

## 🪚 Recompiling New Entry Points (`m68krecomp`)

Neo Geo games often contain computed jumps (jump tables via registers) that static analysis cannot resolve ahead-of-time:

1. **Run headless profiling session to discover entry points:**
   ```bash
   ./build/recomp/kof95 --headless --rom-path roms --frames 10000 --dump-misses entries.txt
   ```
2. **Re-run the generator with `entries.txt`:**
   ```bash
   kof95_recomp --rom-path roms --out generated/ --files 16 --entries entries.txt
   ```
3. **Compile generated sources:**
   Add `/bigobj` in MSVC for large C files containing translated function tables.

---

## 🔒 Copyright & Legal Best Practices for Git Repos

When publishing static recompilation repositories:
1. **Never commit ROM dumps:** `.gitignore` must exclude `roms/`, `*.bin`, `*.zip`, `*.7z`.
2. **Never commit audio or video assets:** Exclude `cdda/`, `*.wav`, `*.mp3`.
3. **Commit recompiler tooling and C sources only:** The repository must require the user to provide their own dumps to generate or run the game.
4. **Git Identity:** Ensure `git config user.name` and `git config user.email` use your intended GitHub account (or noreply email) prior to publishing.
