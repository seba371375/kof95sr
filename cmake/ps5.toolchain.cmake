#
# ps5.toolchain.cmake — CMake toolchain file for cross-compiling KOF95 to PS5
# using ps5-payload-sdk (https://github.com/ps5-payload-dev/ps5-payload-sdk).
#
# Usage:
#   cmake -S . -B build/ps5 \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/ps5.toolchain.cmake \
#         -DKOF95_ROM_DIR=/path/to/roms \
#         -DPS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk
#

cmake_minimum_required(VERSION 3.20)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

if(NOT DEFINED PS5_PAYLOAD_SDK)
    if(DEFINED ENV{PS5_PAYLOAD_SDK})
        set(PS5_PAYLOAD_SDK $ENV{PS5_PAYLOAD_SDK})
    else()
        set(PS5_PAYLOAD_SDK "/opt/ps5-payload-sdk" CACHE PATH "Path to ps5-payload-sdk")
    endif()
endif()

# Compiler and tools
set(CMAKE_C_COMPILER   clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_AR           llvm-ar)
set(CMAKE_RANLIB       llvm-ranlib)

# PS5 Target Flags
set(PS5_TARGET_TRIPLE "x86_64-scei-ps5")
set(PS5_SYSROOT "${PS5_PAYLOAD_SDK}/target")

set(CMAKE_C_FLAGS_INIT   "-target ${PS5_TARGET_TRIPLE} --sysroot=${PS5_SYSROOT} -fno-stack-protector -fPIC")
set(CMAKE_CXX_FLAGS_INIT "-target ${PS5_TARGET_TRIPLE} --sysroot=${PS5_SYSROOT} -fno-stack-protector -fPIC")

# Search paths for libraries and headers
set(CMAKE_FIND_ROOT_PATH "${PS5_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# PS5 homebrew includes & libraries
include_directories(SYSTEM
    "${PS5_SYSROOT}/include"
    "${PS5_SYSROOT}/include/SDL2"
)

link_directories(
    "${PS5_SYSROOT}/lib"
)

add_definitions(
    -D__PS5__
    -D__PLAYSTATION5__
    -DNG_PS5_HOMEBREW
)
