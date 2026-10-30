#
# vita.toolchain.cmake — CMake toolchain file for cross-compiling KOF95 to PS Vita
# using VitaSDK (https://vitasdk.org).
#
# Usage:
#   # 1. First generate recompiled C sources on the host (if using KOF95_ROM_DIR):
#   cmake -S . -B build/recomp -DCMAKE_BUILD_TYPE=Release -DKOF95_ROM_DIR=/path/to/roms
#   cmake --build build/recomp --target kof95_recomp m68kmake
#
#   # 2. Cross-compile for PS Vita and package kof95.vpk:
#   cmake -S . -B build/vita \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/vita.toolchain.cmake \
#         -DKOF95_ROM_DIR=/path/to/roms \
#         -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/vita -j$(nproc)
#

cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED VITASDK)
    if(DEFINED ENV{VITASDK})
        set(VITASDK "$ENV{VITASDK}" CACHE PATH "Path to VitaSDK")
    elseif(EXISTS "/usr/local/vitasdk")
        set(VITASDK "/usr/local/vitasdk" CACHE PATH "Path to VitaSDK")
    elseif(EXISTS "/opt/vitasdk")
        set(VITASDK "/opt/vitasdk" CACHE PATH "Path to VitaSDK")
    elseif(EXISTS "$ENV{HOME}/vitasdk")
        set(VITASDK "$ENV{HOME}/vitasdk" CACHE PATH "Path to VitaSDK")
    else()
        message(FATAL_ERROR "VITASDK not found. Set the VITASDK environment variable or pass -DVITASDK=/path/to/vitasdk")
    endif()
endif()

include("${VITASDK}/share/vita.toolchain.cmake")
