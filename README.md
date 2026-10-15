# KOF '95 Static Recompilation (`kof95sr`)

Native static recompilation and source port of **The King of Fighters '95** (SNK Neo Geo MVS / AES) for modern platforms (**macOS, Linux, Windows, PlayStation 5**).

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

## 📁 ROM Requirements

> [!IMPORTANT]
> This repository **does not contain any copyrighted ROMs, BIOS, or game assets**. You must provide your own legally obtained dumps from the arcade MVS or AES cartridge.

Place your unzipped ROM files inside a folder named `roms/` in the project root:

1. **Neo Geo MVS BIOS (`neogeo.zip`):**
   - `sp-s2.sp1` (or standard MVS system BIOS)
   - `sfix.sfix`
   - `sm1.sm1`
   - `000-lo.lo`
2. **KOF '95 MVS ROMs (`kof95.zip`):**
   - `084-p1.p1`, `084-p2.sp2` (Program ROMs)
   - `084-s1.s1` (Fix ROM)
   - `084-m1.m1` (Z80 Sound ROM)
   - `084-v1.v1`, `084-v2.v2`, `084-v3.v3` (ADPCM Voice/Sample ROMs)
   - `084-c1.c1` .. `084-c8.c8` (Graphic Tiles ROMs)

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

#### Linux & macOS:
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
./build/recomp/kof95

# On Linux Wayland (force X11 / XWayland):
SDL_VIDEODRIVER=x11 ./build/recomp/kof95

# Custom window scale (default: 3)
./build/recomp/kof95 --scale 4

# Force graphics renderer (opengl / sdl / vulkan)
NG_RENDERER=opengl ./build/recomp/kof95
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

## 📜 License & Credits

- The King of Fighters '95 is © SNK Corporation.
- `ymfm` Yamaha sound emulation core is © Aaron Giles (BSD 3-Clause).
- `Musashi` Motorola 68000 core is © Karl Stenerud (MIT).
- Static recompiler architecture based on `neogeorecomp`.
