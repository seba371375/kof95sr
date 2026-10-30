#!/usr/bin/env bash
set -e

# ==============================================================================
# Script de prueba rápida para audio de KOF '95 (ESAKA BGM + Kyo Kusanagi SFX)
# ==============================================================================

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

echo "=========================================================="
echo "  KOF '95 - Test de Audio Oficial YM2610 (ESAKA & Kyo)    "
echo "=========================================================="
echo ""
echo "Opciones:"
echo "  1) Reproducir muestra generada de ESAKA y Kyo (8 seg de pelea) [afplay]"
echo "  2) Reproducir muestra del Intro oficial de Neo Geo [afplay]"
echo "  3) Ejecutar juego en ventana con secuencia automática hasta Esaka (Kyo)"
echo "  4) Generar nuevo archivo WAV de 30 segundos (NG_DUMP_AUDIO=1)"
echo ""

MODE="${1:-1}"

case "$MODE" in
    1|--esaka)
        echo "[Audio] Reproduciendo 'esaka_match.wav' (ESAKA Stage BGM + Kyo en combate)..."
        if [ ! -f "esaka_match.wav" ]; then
            echo "[Audio] Generando esaka_match.wav primero..."
            NG_DUMP_AUDIO=1 ./build/recomp/kof95 --headless --frames 1800 --input tests/test_esaka_kyo.txt >/dev/null 2>&1
            python3 -c "
import wave
with wave.open('kof95_dump.wav', 'rb') as r:
    params = r.getparams()
    r.setpos(22 * 48000)
    frames = r.readframes(8 * 48000)
with wave.open('esaka_match.wav', 'wb') as w:
    w.setparams(params)
    w.writeframes(frames)
"
        fi
        afplay esaka_match.wav
        ;;
    2|--intro)
        echo "[Audio] Reproduciendo 'test_intro.wav' (Neo Geo Intro Theme)..."
        afplay test_intro.wav
        ;;
    3|--live)
        echo "[Audio] Iniciando KOF '95 en ventana con secuencia automática hacia Esaka / Kyo..."
        ./build/recomp/kof95 --frames 1800 --input tests/test_esaka_kyo.txt
        ;;
    4|--dump)
        echo "[Audio] Generando kof95_dump.wav (30 segundos completos)..."
        NG_DUMP_AUDIO=1 ./build/recomp/kof95 --headless --frames 1800 --input tests/test_esaka_kyo.txt
        echo "[Audio] Archivo kof95_dump.wav generado con éxito."
        afplay kof95_dump.wav
        ;;
    *)
        echo "Uso: $0 [1|2|3|4]"
        exit 1
        ;;
esac

echo ""
echo "¡Prueba de audio completada!"
