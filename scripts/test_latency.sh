#!/usr/bin/env bash
set -euo pipefail

SCRIPT_PATH="${BASH_SOURCE[0]}"
while [ -L "$SCRIPT_PATH" ]; do
    SCRIPT_DIR="$(cd -P "$(dirname "$SCRIPT_PATH")" && pwd)"
    SCRIPT_PATH="$(readlink "$SCRIPT_PATH")"
    [[ $SCRIPT_PATH != /* ]] && SCRIPT_PATH="$SCRIPT_DIR/$SCRIPT_PATH"
done
ROOT_DIR="$(cd -P "$(dirname "$SCRIPT_PATH")/.." && pwd)"

FBNEO_SDL_BIN="$ROOT_DIR/build/emulator/fbneo_core/fbneo_sdl"
FBNEO_CORE="$ROOT_DIR/build/emulator/fbneo_core/fbneo_libretro.dylib"
FBNEO_APP="$ROOT_DIR/build/emulator/FinalBurnNeo.app"
FBNEO_ROMS="$HOME/Library/Application Support/FBNeo/roms"

NATIVE_BIN="$ROOT_DIR/build/bin/kof95"

VITA3K_BIN="$ROOT_DIR/build/emulator/Vita3K.app/Contents/MacOS/Vita3K"
VITA3K_DIR="$HOME/Library/Application Support/Vita3K/Vita3K"

# Permite que todos los emuladores reciban eventos del mismo joystick simultáneamente
# incluso cuando una de las ventanas no tiene el foco activo en macOS.
export SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1

# Evita que FinalBurnNeo.app se pause si pierde el foco
defaults write org.akop.FinalBurnNeo pauseWhenInactive -bool false 2>/dev/null || true

ensure_roms() {
    mkdir -p "$FBNEO_ROMS"
    if [ -f "$ROOT_DIR/roms/kof95.zip" ] && [ ! -f "$FBNEO_ROMS/kof95.zip" ]; then
        cp -f "$ROOT_DIR/roms/kof95.zip" "$FBNEO_ROMS/"
    fi
    if [ -f "$ROOT_DIR/roms/neogeo.zip" ] && [ ! -f "$FBNEO_ROMS/neogeo.zip" ]; then
        cp -f "$ROOT_DIR/roms/neogeo.zip" "$FBNEO_ROMS/"
    fi
}

ensure_ipc() {
    mkdir -p "$VITA3K_DIR/ux0/data/kof95"
    if [ ! -f "$VITA3K_DIR/ux0/data/kof95/p2_input.bin" ]; then
        printf '\x00\x00\x00\x00' > "$VITA3K_DIR/ux0/data/kof95/p2_input.bin"
    fi
}

launch_neogeo() {
    ensure_roms
    ensure_ipc
    echo "=========================================================="
    echo " 🕹️  Lanzando FinalBurn Neo (SDL2 + Libretro Core)"
    echo "    ROM:  $ROOT_DIR/roms/kof95.zip"
    echo "    BIOS: $ROOT_DIR/roms/neogeo.zip"
    echo "    Gamepad en segundo plano: HABILITADO"
    echo "=========================================================="
    if [ -x "$FBNEO_SDL_BIN" ] && [ -f "$FBNEO_CORE" ]; then
        "$FBNEO_SDL_BIN" "$FBNEO_CORE" "$ROOT_DIR/roms/kof95.zip"
    else
        open -a "$FBNEO_APP" "$FBNEO_ROMS/kof95.zip"
    fi
}

launch_native() {
    ensure_ipc
    if [ ! -x "$NATIVE_BIN" ]; then
        echo "Compilando versión nativa de macOS..."
        "$ROOT_DIR/scripts/build_native.sh"
    fi
    echo "=========================================================="
    echo " ⚡ Lanzando KOF '95 Nativo Recompilado (macOS / PC)"
    echo "    Binario: $NATIVE_BIN"
    echo "    ROMs:    $ROOT_DIR/roms/"
    echo "    Gamepad en segundo plano: HABILITADO"
    echo "=========================================================="
    NG_WINDOW_X="${NG_WINDOW_X:-720}" NG_WINDOW_Y="${NG_WINDOW_Y:-100}" "$NATIVE_BIN" --rom-path "$ROOT_DIR/roms"
}

launch_vita() {
    ensure_ipc
    echo "=========================================================="
    echo " 🎮 Lanzando Vita3K (PS Vita - KOF950001)"
    echo "    App:  $VITA3K_DIR/ux0/app/KOF950001/eboot.bin"
    echo "    ROMs: $VITA3K_DIR/ux0/data/kof95/roms/"
    echo "    Gamepad en segundo plano: HABILITADO"
    echo "=========================================================="
    "$VITA3K_BIN" -r KOF950001
}

MODE="${1:-}"

if [ -z "$MODE" ]; then
    echo "=========================================================="
    echo "   KOF '95 — Comparador de Latencia vs Neo Geo (FBNeo)"
    echo "=========================================================="
    echo "  1) Neo Geo vs Nativo PC/Mac   (Comparar recompilación nativa)"
    echo "  2) Neo Geo vs PS Vita         (Comparar con emulador Vita3K)"
    echo "  3) Neo Geo vs Ambos           (Nativo PC/Mac + PS Vita)"
    echo "  4) Solo Nativo PC/Mac"
    echo "  5) Solo Emulador PS Vita      (Vita3K)"
    echo "  6) Solo Emulador Neo Geo      (FinalBurn Neo)"
    echo "  q) Salir"
    echo "----------------------------------------------------------"
    read -rp "Selecciona una opción [1/2/3/4/5/6/q]: " choice
    case "$choice" in
        1) MODE="native" ;;
        2) MODE="vita" ;;
        3) MODE="both" ;;
        4) MODE="native_only" ;;
        5) MODE="vita_only" ;;
        6) MODE="neogeo_only" ;;
        *) exit 0 ;;
    esac
fi

case "$MODE" in
    native|mac|pc|1)
        echo "=========================================================="
        echo " 🔥 Comparación: Neo Geo vs Nativo PC/Mac"
        echo "    P1: 1 Joystick controla ambos a la vez en tiempo real"
        echo "    P2: Teclado espejado (W/S/A/D, U/I/O/P, 2=Start, 6=Coin)"
        echo "=========================================================="
        launch_neogeo &
        FBNEO_PID=$!
        trap 'kill "$FBNEO_PID" 2>/dev/null || true' EXIT INT TERM
        sleep 1
        NG_WINDOW_X=720 NG_WINDOW_Y=100 launch_native
        ;;
    vita|psvita|vita3k|2)
        echo "=========================================================="
        echo " 🔥 Comparación: Neo Geo vs PS Vita (Vita3K)"
        echo "    P1: 1 Joystick controla ambos a la vez en tiempo real"
        echo "    P2: Teclado espejado (W/S/A/D, U/I/O/P, 2=Start, 6=Coin)"
        echo "=========================================================="
        launch_neogeo &
        FBNEO_PID=$!
        trap 'kill "$FBNEO_PID" 2>/dev/null || true' EXIT INT TERM
        sleep 1
        launch_vita
        ;;
    both|all|3)
        echo "=========================================================="
        echo " 🔥 Comparación Triple: Neo Geo vs Nativo PC/Mac vs PS Vita"
        echo "    P1: 1 Joystick controla los tres a la vez"
        echo "    P2: Teclado espejado para todos"
        echo "=========================================================="
        launch_neogeo &
        FBNEO_PID=$!
        sleep 1
        NG_WINDOW_X=700 NG_WINDOW_Y=50 launch_native &
        NATIVE_PID=$!
        trap 'kill "$FBNEO_PID" "$NATIVE_PID" 2>/dev/null || true' EXIT INT TERM
        sleep 1
        launch_vita
        ;;
    native_only|4)
        launch_native
        ;;
    vita_only|5)
        launch_vita
        ;;
    neogeo|fbneo|neogeo_only|6)
        launch_neogeo
        ;;
    *)
        echo "Uso: $0 [native | vita | both | native_only | vita_only | neogeo]"
        echo "  native : Comparar FinalBurn Neo vs KOF '95 Nativo (PC/Mac)"
        echo "  vita   : Comparar FinalBurn Neo vs PS Vita (Vita3K)"
        echo "  both   : Lanzar FinalBurn Neo + Nativo PC/Mac + PS Vita"
        exit 1
        ;;
esac
