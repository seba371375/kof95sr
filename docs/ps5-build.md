# Guía de Compilación de KOF95 para PS5

Esta guía describe cómo compilar el ejecutable nativo de *The King of Fighters '95* para PlayStation 5 usando el entorno estándar de homebrew **ps5-payload-sdk**.

---

## 1. Requisitos para PS5

1. **Toolchain PS5**:
   - [ps5-payload-sdk](https://github.com/ps5-payload-dev/ps5-payload-sdk)
   - LLVM / Clang con soporte para x86_64 ELF
2. **Bibliotecas ported para PS5**:
   - SDL2 para PS5 (`ps5-payload-dev/SDL`)
   - Driver Vulkan / EGL del SDK
3. **ROMs**:
   - Los archivos de ROMs del juego (`084-*.bin`) y la BIOS (`sp-s2.sp1`) ubicados en una carpeta accesible por la consola (ej. `/data/kof95/roms` o empaquetados junto al payload).

---

## 2. Flujo de Compilación (Cross-compilation)

El recompiler de KOF95 (`kof95_recomp`) es una herramienta de host (se corre en tu PC/Mac para generar los archivos C desde los ROMs).
El juego (`kof95`) es el ejecutable que se compila con el toolchain de PS5:

### Paso 1: Generar el código C en tu máquina host
```bash
# Ya generado en build/recomp/generated/ (más de 695,000 instrucciones de KOF95)
```

### Paso 2: Cross-compilar para PS5 usando CMake
```bash
cmake -S . -B build/ps5 \
      -DCMAKE_TOOLCHAIN_FILE=cmake/ps5.toolchain.cmake \
      -DPS5_PAYLOAD_SDK=/opt/ps5-payload-sdk \
      -DKOF95_ROM_DIR=$PWD/roms

cmake --build build/ps5 --config Release
```

El resultado es un binario ELF para PS5 (`kof95.elf`) que se envía vía payload loader (por ejemplo vía el exploit de WebKit o BD-J con `nc <ps5-ip> 9020 < kof95.elf`).

---

## 3. Controles en PS5 (DualSense / DualShock 4)

El motor cuenta con soporte nativo de `SDL_GameController`:

| Botón DualSense / PS5 | Acción KOF95 Arcade |
|---|---|
| **D-Pad / Stick Izquierdo** | Arriba / Abajo / Izquierda / Derecha |
| **Cruz (X)** | Golpe Débil (**A**) |
| **Círculo (O)** | Patada Débil (**B**) |
| **Cuadrado (□)** | Golpe Fuerte (**C**) |
| **Triángulo (△)** | Patada Fuerte (**D**) |
| **L1** | Macro: Esquiva / Rodar (**A + B**) |
| **R1** | Macro: Golpe Fuerte / Blow-away (**C + D**) |
| **L2** | Macro: Cargar Barra de Poder (**A + B + C**) |
| **R2** | Macro: Provocación (**B + C + D**) |
| **Options (Start)** | Start (1P) |
| **Touchpad Click / Create** | Insertar Moneda (Coin) |
| **L3 (Stick Click)** | Select |
| **R3 (Stick Click)** | Test / Service Switch |

*Un segundo mando DualSense conectado actuará automáticamente como Jugador 2 (P2).*

---

## 4. Renderizado (OpenGL / Vulkan)

- En PS5, el backend de SDL2 utiliza Vulkan / EGL surface directamente sobre el hardware gráfico RDNA2 de la consola.
- Se mantiene la relación de aspecto 4:3 con letterboxing limpio e interpolación nítida de píxeles para recrear la imagen arcade original a 59.1856 Hz sin tearing.
