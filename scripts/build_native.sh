#!/bin/bash
set -e

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

mkdir -p build/bin build/obj_native

echo "=========================================================="
echo " 🔨 Building native KOF '95 Static Recompilation (macOS)"
echo "=========================================================="

CFLAGS="-O2 -w \
  -Isrc -Ibuild/recomp/generated -Ineogeorecomp/include -Ineogeorecomp/src \
  -Ineogeorecomp/ext/musashi -Ibuild/recomp/neogeorecomp_build/musashi_gen \
  -Ineogeorecomp/ext/z80 -Ineogeorecomp/ext/ymfm/src \
  -Fbuild/emulator/fbneo_core -Ibuild/emulator/fbneo_core/SDL2.framework/Headers \
  -DMUSASHI_CNF=\"ng_musashi_conf.h\" -DNG_HAVE_SDL -DNG_HAVE_OPENGL -DKOF95_HAVE_RECOMP"

CXXFLAGS="-O2 -w -std=c++14 \
  -Ineogeorecomp/include -Ineogeorecomp/src -Ineogeorecomp/ext/ymfm/src"

C_SRCS=(
  src/main.c
  src/game.c
  neogeorecomp/src/neogeo.c
  neogeorecomp/src/bus.c
  neogeorecomp/src/cpu.c
  neogeorecomp/src/io.c
  neogeorecomp/src/video.c
  neogeorecomp/src/lspc.c
  neogeorecomp/src/audio.c
  neogeorecomp/src/exec.c
  neogeorecomp/src/platform.c
  neogeorecomp/src/romload.c
  neogeorecomp/src/window_sdl.c
  neogeorecomp/src/osd_menu.c
  neogeorecomp/src/musashi_glue.c
  neogeorecomp/ext/musashi/m68kcpu.c
  neogeorecomp/ext/musashi/m68kdasm.c
  neogeorecomp/ext/musashi/softfloat/softfloat.c
  build/recomp/neogeorecomp_build/musashi_gen/m68kops.c
  neogeorecomp/ext/z80/z80.c
  build/recomp/generated/recomp_table.c
)

for f in build/recomp/generated/recomp_*.c; do
  if [ "$f" != "build/recomp/generated/recomp_table.c" ]; then
    C_SRCS+=("$f")
  fi
done

CXX_SRCS=(
  neogeorecomp/src/ym2610_bridge.cpp
  neogeorecomp/ext/ymfm/src/ymfm_opn.cpp
  neogeorecomp/ext/ymfm/src/ymfm_adpcm.cpp
  neogeorecomp/ext/ymfm/src/ymfm_ssg.cpp
)

# Parallel compile
PIDS=()
for src in "${C_SRCS[@]}"; do
  obj="build/obj_native/$(basename "${src%.*}").o"
  if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
    clang $CFLAGS -c "$src" -o "$obj" &
    PIDS+=($!)
  fi
done

for src in "${CXX_SRCS[@]}"; do
  obj="build/obj_native/$(basename "${src%.*}").o"
  if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
    clang++ $CXXFLAGS -c "$src" -o "$obj" &
    PIDS+=($!)
  fi
done

if [ ${#PIDS[@]} -gt 0 ]; then
  echo "Compiling ${#PIDS[@]} objects in parallel..."
  for pid in "${PIDS[@]}"; do
    wait "$pid"
  done
fi

echo "Linking build/bin/kof95..."
clang++ build/obj_native/*.o \
  -Fbuild/emulator/fbneo_core -framework SDL2 -framework OpenGL -framework ApplicationServices -lm \
  -Wl,-rpath,"$PWD/build/emulator/fbneo_core" \
  -o build/bin/kof95

echo "=========================================================="
echo " ✅ Build complete: build/bin/kof95"
echo "=========================================================="
ls -lh build/bin/kof95
