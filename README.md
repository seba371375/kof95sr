# KOF '95 Static Recompilation (`kof95sr`)

Native static recompilation and source port of **The King of Fighters '95** (SNK Neo Geo MVS / AES) for modern platforms (**macOS, Linux, Windows, PlayStation 5, PlayStation Vita**).

> **Architected and coded by [Antigravity](https://github.com/google-deepmind)**  
> *Pair-programmed with [seba371375](https://github.com/seba371375)*

Instead of traditional full-system emulation, `kof95sr` translates the original Motorola 68000 arcade assembly ahead-of-time (AOT) into native C routines, executing them directly on the host CPU with zero interpreter overhead.

---

## ✨ Features

- **⚡ Native Performance:** Static recompilation of 28,000+ entry points into native C functions compiled with `-O3` optimization (Clang / GCC / MSVC). Runs at locked 60 FPS on PC and natively on the PS5's AMD Zen 2 CPU.
- **🔊 Bit-Perfect Sound:** Authentic **Yamaha YM2610** (OPNB) FM / SSG / ADPCM audio synthesis powered by `ymfm` running at native 55.5 kHz / 48 kHz stereo, synchronized with the Z80 sound CPU and authentic attract demo sound.
- **🖥️ Display & Video Filters:**
  - Neo Geo LSPC scanline-by-scanline raster renderer with pixel-accurate sprite zoom, palette banking, 4:3 arcade aspect ratio, integer scaling, and letterboxing via SDL2 Accelerated or OpenGL.
  - Real-time video post-processing filters: **Raw Pixel (Nearest)**, **Bilinear (Smooth)**, **CRT Scanlines (50% & 75%)**, **CRT Trinitron (Aperture Grille RGB)**, **Scale2x (Pixel HD)**, and **CRT Warm (Curved)**.
  - Native 1920x1080 fullscreen mode on PlayStation 5 with hardware VSync lock.
- **🎮 Controller & Input Support:**
  - Full hotplug support for DualSense (PS5), DualShock 4 (PS4), Xbox, Switch Pro, fightsticks, and keyboard.
  - Independent controller mapping ensuring Player 1 and Player 2 are never duplicated on multi-interface devices.
  - **3 Gamepad Layout Presets** selectable anytime in-game:
    - `0: SNK Default` (Cross=A, Circle=B, Square=C, Triangle=D)
    - `1: Fighting Modern` (Square=A, Triangle=C, Cross=B, Circle=D)
    - `2: KOF Classic Line` (Square=A, Cross=B, Triangle=C, R1=D)
  - Dedicated macro shortcuts: <kbd>L1</kbd> (Emergency Roll / Dodge), <kbd>R1</kbd> (Blow-Away Attack), <kbd>L2</kbd> (Power Charge), <kbd>R2</kbd> (Taunt).
- **⏸️ Interactive In-Game OSD Menu with Pause:**
  - Press <kbd>L3</kbd> + <kbd>R3</kbd> (or <kbd>L1</kbd> + <kbd>R1</kbd> + <kbd>Options</kbd>) on gamepad, or <kbd>Tab</kbd> / <kbd>F1</kbd> on keyboard.
  - **Automatic Emulation & Audio Pause:** Opening the menu cleanly freezes the game simulation, timer, and audio output. Resuming restores gameplay seamlessly.
  - **DIP Switch Settings:** Adjust game difficulty (Level 1 to 8), arcade blood color (Red/Off), Free Play, and Coin Chutes.
  - **Keyboard Rebinding Submenu:** Remap Player 1 and Player 2 keyboard actions directly from the GUI.
  - **Full Persistence:** Configurations automatically save to `kof95_dips.cfg` and `kof95_controls.cfg` (in the local folder on PC, or `/data/kof95/` on PS5).
- **⭐ Gameplay Enhancements:**
  - **Duplicate Characters:** Optional Team Edit setting allowing duplicate fighters on the same team (e.g., triple Kyo, triple Iori).
  - **Random Select `[?]` Slot:** Interactive random slot added to the character selection grid, confirmable with any attack button with authentic stage re-roll.

---

## 📁 ROM Requirements & Placement

> [!IMPORTANT]
> This repository **does not contain any copyrighted ROMs, BIOS, or game assets**. You must provide your own legally obtained dumps from an authentic SNK Neo Geo MVS arcade or AES cartridge.

### 1. File List (Total 18 unzipped files)

`kof95sr` automatically detects both NeoRAGEx/dump naming (`.bin`) and standard MAME naming (`.p1`, `.c1`, etc.):

| Category | File Names (MAME / `.bin`) | Size | Description |
| :--- | :--- | :--- | :--- |
| **Program (CPU)** | `084-p1.p1` or `084-p1.bin` | 2 MB | 68000 main program code *(Note: KOF '95 only uses a single P ROM)* |
| **Fix / Text** | `084-s1.s1` or `084-s1.bin` | 128 KB | Text, HUD, and menu layer graphics |
| **Sound CPU** | `084-m1.m1` or `084-m1.bin` | 128 KB | Z80 sound sub-CPU driver program |
| **Voices / ADPCM** | `084-v1.v1`, `084-v2.v2`, `084-v3.v3`<br>*(or `084-v1.bin`, `084-v2.bin`, `084-v3.bin`)* | 4MB, 2MB, 1MB | YM2610 ADPCM sample and voice tracks |
| **Sprite Tiles** | `084-c1.c1` .. `084-c8.c8`<br>*(or `084-c1.bin` .. `084-c8.bin`)* | 8 files (1-4 MB) | Character sprites and background graphics |
| **Neo Geo BIOS** | `sp-s2.sp1` *(or `asia-s3.rom`, `uni-bios_4_0.rom`)*<br>`sfix.sfix`<br>`sm1.sm1`<br>`000-lo.lo` | 128 KB each | Neo Geo MVS/AES system BIOS and font ROMs |

---

### 2. ROM Placement by Platform

Place all 18 unzipped files into the path corresponding to your platform:

| Platform | ROM Directory Path | Notes |
| :--- | :--- | :--- |
| **macOS / Linux / Windows** | `./roms/` (in the project/run folder) | Can be customized via `--rom-path /path/to/roms` |
| **PlayStation 5** | `/data/kof95/roms/` | Transfer via FTP (`/data/kof95/roms/`). Fallbacks: `/data/kof95/`, `/user/homebrew/kof95/roms/` |
| **PlayStation Vita** | `ux0:data/kof95/roms/` | Transfer via VitaShell FTP/USB. Fallbacks: `ux0:data/kof95/`, `uma0:data/kof95/roms/` |

---

## 💻 Building & Running on PC (Linux, macOS, Windows)

### Prerequisites

* **Ubuntu / Debian / Nobara / Fedora:**
  ```bash
  sudo apt-get install build-essential cmake ninja-build libsdl2-dev zlib1g-dev libgl1-mesa-dev
  # Or on Fedora/Nobara:
  sudo dnf install gcc gcc-c++ cmake ninja-build SDL2-devel mesa-libGL-devel
  ```
* **macOS:**
  ```bash
  brew install cmake ninja sdl2
  ```
* **Windows (Visual Studio 2022 / MSVC):**
  - Install Visual Studio 2022 with Desktop C++ and CMake.
  - Install SDL2 with [vcpkg](https://github.com/microsoft/vcpkg):
    ```powershell
    vcpkg install sdl2:x64-windows
    ```

### Compilation

#### Quick Native Build (macOS & Linux):
```bash
# Direct fast build without CMake (outputs to build/bin/kof95)
./scripts/build_native.sh
```

#### Via CMake & Ninja:
```bash
# Configure with Release optimizations (-O3)
cmake -S . -B build/recomp -G Ninja -DCMAKE_BUILD_TYPE=Release -DKOF95_ROM_DIR=roms

# Build executable
ninja -C build/recomp
```

#### Windows:
```powershell
cmake -S . -B build/recomp -DCMAKE_BUILD_TYPE=Release -DKOF95_ROM_DIR=roms -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build/recomp --config Release
```

### Running on PC

```bash
# Standard launch
./build/bin/kof95
# (or ./build/recomp/kof95 if built via CMake)

# On Linux Wayland (force X11 / XWayland):
SDL_VIDEODRIVER=x11 ./build/bin/kof95

# Custom window scale (default: 3)
./build/bin/kof95 --scale 4

# Force graphics renderer (opengl / sdl / vulkan)
NG_RENDERER=opengl ./build/bin/kof95
```

### ⚡ Side-by-Side Latency Verification Suite

Compare input response latency side-by-side with original Neo Geo MVS arcade emulation (FinalBurn Neo core) with synchronized inputs:

```bash
# Compare native macOS/PC recompilation vs Neo Geo arcade:
./scripts/test_latency.sh native

# Compare PS Vita (via Vita3K) vs Neo Geo arcade:
./scripts/test_latency.sh vita
```

---

## 🎮 Building & Running on PlayStation 5 (Native Homebrew)

The game compiles directly as a native PS5 ELF payload running on bare metal with DualSense controller support and hardware VSync.

### PS5 Requirements
* A PlayStation 5 console on vulnerable firmware (**3.00 – 4.51**, or WebKit/BD-J exploit compatible).
* An active ELF Loader (e.g. port `9020` / `9021`) or **Homebrew Loader / ItemzFlow**.
* An active FTP server on the console (e.g. `ftpsrv.elf` on port `2121`).

### Cross-Compiling for PS5 on PC
1. Install [ps5-payload-sdk](https://github.com/ps5-payload-dev/ps5-payload-sdk) on your PC (e.g. `~/ps5-payload-sdk`).
2. Build the PS5 target in Release mode:
   ```bash
   cmake -S . -B build/ps5 \
         -DCMAKE_TOOLCHAIN_FILE=cmake/ps5.toolchain.cmake \
         -DKOF95_ROM_DIR=$PWD/roms \
         -DCMAKE_BUILD_TYPE=Release

   cmake --build build/ps5 --target kof95 -j$(nproc)

   # Strip debug symbols
   ~/ps5-payload-sdk/bin/prospero-strip -s build/ps5/kof95 -o build/ps5/kof95.elf
   ```

### Deploying to PS5 via FTP
Transfer the files to your PS5 internal storage:
* **Game ROM files:** Upload the 18 ROM files to `/data/kof95/roms/`
* **Executable:** Upload `build/ps5/kof95.elf` to:
  * `/data/kof95/kof95.elf`
  * `/data/homebrew/kof95/eboot.elf` (for Homebrew Loader / ItemzFlow)

### Launching on PS5
* **Via Homebrew Loader / ItemzFlow:** Launch KOF95 from the homebrew list.
* **Via Netcat / ELF Loader:**
  ```bash
  nc -w 3 <IP_DE_TU_PS5> 9020 < build/ps5/kof95.elf
  ```

---

## 🕹️ Building & Running on PlayStation Vita (VitaDB / `.vpk`)

`kof95sr` builds as a native PS Vita homebrew package (`kof95.vpk`, Title ID `KOF950001`) running at 444 MHz ARM Cortex-A9 with GXM hardware scaling (`960x544`) and extended memory (`ATTRIBUTE2=12` / 192 MB user heap).

### Cross-Compiling `kof95.vpk` with VitaSDK
1. Install [VitaSDK](https://vitasdk.org/) (or use the official `vitasdk/vitasdk:latest` Docker image).
2. Generate the recompiled C sources on the host machine first, then cross-compile for PS Vita:
   ```bash
   # 1. Host build (generates AOT C blocks and Musashi tables)
   cmake -S . -B build/recomp -DCMAKE_BUILD_TYPE=Release -DKOF95_ROM_DIR=$PWD/roms
   cmake --build build/recomp --target kof95 -j$(nproc)

   # 2. PS Vita cross-compilation (.vpk package)
   cmake -S . -B build/vita \
         -DCMAKE_TOOLCHAIN_FILE=cmake/vita.toolchain.cmake \
         -DKOF95_ROM_DIR=$PWD/roms \
         -DCMAKE_BUILD_TYPE=Release
   cmake --build build/vita --target kof95.vpk-vpk -j$(nproc)
   ```

### Installing & Running on PS Vita
1. Install `build/vita/kof95.vpk` on your PS Vita using **VitaShell**.
2. Copy your unzipped MAME `kof95` and `neogeo` BIOS ROM files into:
   ```text
   ux0:data/kof95/roms/
   ```
   *(Both `.bin` and standard MAME `.p1`/`.c1`/`.s1`/`.m1`/`.v1` extensions are detected automatically).*
3. Launch **The King of Fighters '95** from the PS Vita LiveArea.
   - **Insert Coin:** <kbd>SELECT</kbd>
   - **Start Game:** <kbd>START</kbd>
   - **Open / Close OSD Menu:** <kbd>SELECT</kbd> + <kbd>START</kbd> (or <kbd>L</kbd> + <kbd>R</kbd> + <kbd>START</kbd>)
   - **Roll / Dodge (A+B):** <kbd>L</kbd>
   - **Blow-Away Attack (C+D):** <kbd>R</kbd>
   - **Power Charge (A+B+C):** Right Stick Left / Up
   - **Taunt (B+C+D):** Right Stick Right / Down

---

## 🕹️ Controls Reference

| Action | Player 1 (Keyboard) | Player 2 (Keyboard) | DualSense / Gamepad (SNK Preset) |
| :--- | :--- | :--- | :--- |
| **Up / Down / Left / Right** | Arrow Keys | W / S / A / D | D-Pad / Left Stick |
| **A (Light Punch)** | J | Z | Cross (<kbd>✕</kbd>) / A |
| **B (Light Kick)** | K | X | Circle (<kbd>○</kbd>) / B |
| **C (Heavy Punch)** | U | C | Square (<kbd>□</kbd>) / X |
| **D (Heavy Kick)** | I | V | Triangle (<kbd>△</kbd>) / Y |
| **Emergency Roll / Dodge (A + B)** | — | — | <kbd>L1</kbd> |
| **Blow-Away Attack (C + D)** | — | — | <kbd>R1</kbd> |
| **Power Charge (A + B + C)** | — | — | <kbd>L2</kbd> (Left Trigger) |
| **Taunt (B + C + D)** | — | — | <kbd>R2</kbd> (Right Trigger) |
| **Start** | 1 | 2 | Options / Start |
| **Coin (Insert Credit)** | F1 / 5 | 6 | Click Touchpad / <kbd>L3</kbd> / Select |
| **Open / Close OSD Menu (Pause)** | <kbd>Tab</kbd> / <kbd>F1</kbd> | — | **<kbd>L3</kbd> + <kbd>R3</kbd>** or **<kbd>L1</kbd> + <kbd>R1</kbd> + <kbd>Options</kbd>** |
| **Toggle Fullscreen** | <kbd>F11</kbd> / <kbd>Alt</kbd>+<kbd>Enter</kbd> | — | Automatic (Native 1080p on PS5) |
| **Quick Video Filter Toggle** | <kbd>F5</kbd> | — | Selectable inside OSD Menu |

---

## 📚 References & Technical Documentation

This project builds upon foundational hardware reverse engineering and open-source emulation tools:

* **Hardware Documentation & Architecture:**
  * [NeoGeo Development Wiki](https://wiki.neogeodev.org/) — Hardware register specifications, LSPC video timings, memory mapping, and graphics formats.
  * [Neo Geo MVS/AES Technical Information](https://wiki.neogeodev.org/index.php?title=Memory_map) — Cartridge bankswitching and Z80 sound port interfaces.
* **Emulation Cores & Audio Bridges:**
  * [Musashi 680x0 CPU Engine](https://github.com/kstenerud/Musashi) by Karl Stenerud — Used for opcode disassembly, verification, and fallback decoding.
  * [ymfm Yamaha FM/ADPCM Core](https://github.com/aaronsgiles/ymfm) by Aaron Giles — Bit-perfect Yamaha YM2610 (OPNB) sound synthesis.
  * [Z80 CPU Core](https://github.com/redcode/Z80) — Cycle-accurate Zilog Z80 sound CPU emulation.
* **Target Platforms & SDKs:**
  * [Simple DirectMedia Layer (SDL2)](https://www.libsdl.org/) — Cross-platform windowing, input polling, and audio device management.
  * [VitaSDK](https://vitasdk.org/) & [vitaGL](https://github.com/Rinnegatamante/vitaGL) — PlayStation Vita GCC cross-compiler toolchain and GXM graphics library by Rinnegatamante.
  * [ps5-payload-sdk](https://github.com/ps5-payload-dev/ps5-payload-sdk) — PlayStation 5 native bare-metal homebrew toolchain.
* **Static Recompilation Methodology:**
  * Inspired by modern ahead-of-time (AOT) static recompilation techniques seen in projects such as [N64Recomp](https://github.com/Mr-Wiseguy/N64Recomp) and [Zelda 64 Recomp](https://github.com/ZeldaRE/Zelda64Recomp).

---

## 📜 License & Disclaimers

This project is licensed under the **[MIT License](LICENSE)** — see the [LICENSE](LICENSE) file for full details.

* **Codebase Author:** Developed and coded by **[Antigravity](https://github.com/google-deepmind)** (Google DeepMind).
* **Game Copyright:** **The King of Fighters '95** is © 1995 **SNK Corporation**. All rights reserved. This repository contains **NO** copyrighted ROMs, code, artwork, or sound files. Users must provide their own legally acquired game ROMs.
* **Third-Party Licenses:**
  * `ymfm` sound core: © Aaron Giles ([BSD 3-Clause](https://opensource.org/licenses/BSD-3-Clause)).
  * `Musashi` 68000 core: © Karl Stenerud ([MIT](https://opensource.org/licenses/MIT)).
  * `SDL2`: © Sam Lantinga ([zlib License](https://www.zlib.net/zlib_license.html)).
