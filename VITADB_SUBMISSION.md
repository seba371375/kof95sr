Issue title: [NEW] The King of Fighters '95

# Name
The King of Fighters '95

# Version
1.0.0

# Screenshots
<img width="960" height="544" alt="The King of Fighters '95 on PS Vita - Intro" src="https://raw.githubusercontent.com/seba371375/kof95sr/main/screenshots/screenshot_01.jpg" />
<img width="960" height="544" alt="The King of Fighters '95 on PS Vita - Gameplay" src="https://raw.githubusercontent.com/seba371375/kof95sr/main/screenshots/screenshot_02.jpg" />
<img width="960" height="544" alt="The King of Fighters '95 on PS Vita - Character Select" src="https://raw.githubusercontent.com/seba371375/kof95sr/main/screenshots/screenshot_03.jpg" />

# Author
Antigravity, seba371375. (Original arcade game © 1995 SNK Corporation)

# AI status
AI assisted: The static recompilation pipeline, low-level ARM Cortex-A9 optimizations, asynchronous dual-core threading architecture, and PS Vita porting were architected and implemented by Antigravity (Google DeepMind) in pair-programming collaboration with seba371375. Every build was profiled, debugged, and verified on real PlayStation Vita hardware.

# Description
The King of Fighters '95 Static Recompilation (`kof95sr`) is a native, source-level port of SNK's legendary arcade fighting classic for the PlayStation Vita (Title ID: `KOF950001`).

Instead of traditional full-system emulation, `kof95sr` statically recompiles 28,000+ Motorola 68000 arcade assembly entry points ahead-of-time into optimized C routines, executing them directly on the Vita's ARM Cortex-A9 CPU at locked 60 FPS with zero interpreter overhead.

### Key Features:
- **Locked 60 FPS with Hardware VSync:** Powered by hardware GXM rasterization (`960x544`), branchless 64-bit sprite rendering, and non-blocking display pacing.
- **Asynchronous Dual-Core Audio Architecture:** Z80 sound CPU emulation and Yamaha YM2610 (OPNB) FM/SSG/ADPCM synthesis are offloaded to CPU Core 1, completely freeing Core 0 for game logic and graphics.
- **Interactive In-Game OSD Menu & Pause:** Press <kbd>SELECT</kbd> + <kbd>START</kbd> (or <kbd>L</kbd> + <kbd>R</kbd> + <kbd>START</kbd>) anytime to cleanly freeze emulation and adjust DIP switches (difficulty, blood color, duplicate characters, random select `[?]` slot) and video filters.
- **Real-Time Controller Mapping:** Multiple layout presets (SNK Arcade, Fighting Modern, and KOF Classic) plus dedicated shoulder button macro shortcuts (<kbd>L</kbd> for Roll/Dodge, <kbd>R</kbd> for Blow-Away attack, Right Stick for Power Charge and Taunt).
- **Clean-Room & Legal:** Contains zero copyrighted game assets or BIOS files. Requires user-provided original Neo Geo MVS/AES ROM files.

# Short Description
Native static recompilation and source port of The King of Fighters '95 running at locked 60 FPS with dual-core audio.

# Download Link
https://github.com/seba371375/kof95sr/releases/download/v1.0.0/kof95.vpk

# Release Link
https://github.com/seba371375/kof95sr/releases/tag/v1.0.0

# Sourcecode Link
https://github.com/seba371375/kof95sr

# Data Files Link
None bundled. Requires original uncompressed Neo Geo MVS/AES game ROM files (MAME `kof95.zip`) and Neo Geo BIOS files (`neogeo.zip`).

# Requirements
Place the 18 unzipped MAME ROM files and BIOS in `ux0:data/kof95/roms/`:
- `084-p1.p1` (or `084-p1.bin`)
- `084-s1.s1` (or `084-s1.bin`)
- `084-m1.m1` (or `084-m1.bin`)
- `084-v1.v1`, `084-v2.v2`, `084-v3.v3` (or `.bin`)
- `084-c1.c1` through `084-c8.c8` (or `.bin`)
- `sp-s2.sp1`, `sfix.sfix`, `sm1.sm1`, `000-lo.lo`
