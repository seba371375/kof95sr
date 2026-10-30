cmake_minimum_required(VERSION 3.20)
project(neogeorecomp VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Tests build by default only when this is the top-level project, not when
# a game pulls the toolkit in with add_subdirectory().
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(ng_tests_default ON)
else()
    set(ng_tests_default OFF)
endif()
option(NEOGEORECOMP_BUILD_TESTS "Build the conformance harness" ${ng_tests_default})

set(NG_EXT ${CMAKE_CURRENT_SOURCE_DIR}/ext)
if(NOT EXISTS ${NG_EXT}/musashi/m68k_in.c OR NOT EXISTS ${NG_EXT}/z80/z80.c)
    message(FATAL_ERROR "Submodules missing: run  git submodule update --init")
endif()

# ---- Musashi (MIT): 68000 interpreter, the fallback and the oracle ----
# m68kmake generates the opcode handlers from m68k_in.c at build time.
add_executable(m68kmake ${NG_EXT}/musashi/m68kmake.c)
if(MSVC)
    target_compile_definitions(m68kmake PRIVATE _CRT_SECURE_NO_WARNINGS)
    # MSVC 19.4x at /O2 miscompiles m68kmake's mask computation (bit 14 of
    # every opcode mask is lost, e.g. 0xb000 for 0xf000), which crashes
    # m68k_init. It is a one-shot generator, so build it unoptimized.
    target_compile_options(m68kmake PRIVATE /Od)
endif()
set(MUSASHI_GEN ${CMAKE_CURRENT_BINARY_DIR}/musashi_gen)
file(MAKE_DIRECTORY ${MUSASHI_GEN})
if(CMAKE_CROSSCOMPILING)
    # When cross-compiling, use host-built m68kmake or copy from host build if available
    set(HOST_MUSASHI_GEN "${CMAKE_SOURCE_DIR}/build/recomp/neogeorecomp_build/musashi_gen")
    set(HOST_M68KMAKE "${CMAKE_SOURCE_DIR}/build/recomp/neogeorecomp_build/m68kmake")
    if(EXISTS "${HOST_MUSASHI_GEN}/m68kops.c")
        add_custom_command(
            OUTPUT ${MUSASHI_GEN}/m68kops.c ${MUSASHI_GEN}/m68kops.h
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${HOST_MUSASHI_GEN} ${MUSASHI_GEN}
            COMMENT "Copying pregenerated Musashi opcode tables from host build")
    elseif(EXISTS "${HOST_M68KMAKE}")
        add_custom_command(
            OUTPUT ${MUSASHI_GEN}/m68kops.c ${MUSASHI_GEN}/m68kops.h
            COMMAND ${HOST_M68KMAKE} ${MUSASHI_GEN} ${NG_EXT}/musashi/m68k_in.c
            DEPENDS ${NG_EXT}/musashi/m68k_in.c
            COMMENT "Generating Musashi opcode tables using host m68kmake")
    else()
        find_program(HOST_CC NAMES cc gcc clang REQUIRED)
        add_custom_command(
            OUTPUT ${MUSASHI_GEN}/m68kops.c ${MUSASHI_GEN}/m68kops.h
            COMMAND ${HOST_CC} ${NG_EXT}/musashi/m68kmake.c -o ${CMAKE_CURRENT_BINARY_DIR}/m68kmake_host
            COMMAND ${CMAKE_CURRENT_BINARY_DIR}/m68kmake_host ${MUSASHI_GEN} ${NG_EXT}/musashi/m68k_in.c
            DEPENDS ${NG_EXT}/musashi/m68k_in.c
            COMMENT "Building and running host m68kmake for cross-compilation")
    endif()
else()
    add_custom_command(
        OUTPUT ${MUSASHI_GEN}/m68kops.c ${MUSASHI_GEN}/m68kops.h
        COMMAND m68kmake ${MUSASHI_GEN} ${NG_EXT}/musashi/m68k_in.c
        DEPENDS m68kmake ${NG_EXT}/musashi/m68k_in.c
        COMMENT "Generating Musashi opcode tables")
endif()

add_library(musashi STATIC
    ${NG_EXT}/musashi/m68kcpu.c
    ${NG_EXT}/musashi/m68kdasm.c
    ${NG_EXT}/musashi/softfloat/softfloat.c
    ${MUSASHI_GEN}/m68kops.c
    src/musashi_glue.c)
target_include_directories(musashi PUBLIC ${NG_EXT}/musashi ${MUSASHI_GEN} ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(musashi PUBLIC MUSASHI_CNF="ng_musashi_conf.h")
if(MSVC)
    target_compile_definitions(musashi PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(musashi PRIVATE /w)
else()
    target_compile_options(musashi PRIVATE -w -O1)
    if(NOT PS5)
        target_link_libraries(musashi PUBLIC m)   # the FPU code calls sin/cos
    endif()
endif()

# ---- Z80 (MIT) ----
add_library(z80core STATIC ${NG_EXT}/z80/z80.c)
target_include_directories(z80core PUBLIC ${NG_EXT}/z80)
if(MSVC)
    target_compile_options(z80core PRIVATE /w)
endif()

# ---- runtime ----
# SDL2 provides the window; without it the runtime builds headless-only.
find_package(SDL2 CONFIG QUIET)

add_library(neogeorecomp STATIC
    src/neogeo.c
    src/exec.c
    src/cpu.c
    src/bus.c
    src/lspc.c
    src/video.c
    src/io.c
    src/audio.c
    src/platform.c
    src/romload.c
    src/ym2610_bridge.cpp
    ${NG_EXT}/ymfm/src/ymfm_opn.cpp
    ${NG_EXT}/ymfm/src/ymfm_adpcm.cpp
    ${NG_EXT}/ymfm/src/ymfm_ssg.cpp)
target_include_directories(neogeorecomp
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${NG_EXT}/ymfm/src)
target_link_libraries(neogeorecomp PUBLIC musashi z80core)
if(WIN32)
    target_link_libraries(neogeorecomp PUBLIC dbghelp winmm)
endif()
if(VITA)
    target_sources(neogeorecomp PRIVATE src/window_sdl.c src/osd_menu.c)
    target_compile_definitions(neogeorecomp PRIVATE NG_HAVE_SDL)
    target_include_directories(neogeorecomp PUBLIC "${VITASDK}/arm-vita-eabi/include/SDL2")
    target_link_libraries(neogeorecomp PUBLIC
        SDL2
        vitaGL
        vitashark
        SceShaccCgExt
        taihen_stub
        mathneon
        SceShaccCg_stub
        SceKernelDmacMgr_stub
        SceIme_stub
        SceGxm_stub
        SceDisplay_stub
        SceCtrl_stub
        SceAppMgr_stub
        SceAppUtil_stub
        SceAudio_stub
        SceAudioIn_stub
        SceSysmodule_stub
        SceIofilemgr_stub
        SceCommonDialog_stub
        SceTouch_stub
        SceHid_stub
        SceMotion_stub
        ScePower_stub
        SceProcessmgr_stub
        m
        pthread
    )
    message(STATUS "neogeorecomp: PS Vita SDL2 / GXM backend enabled")
elseif(TARGET SDL2::SDL2)
    # Visible to the game's CMakeLists too, so it can ship the DLL (if not an ALIAS).
    get_target_property(is_alias SDL2::SDL2 ALIASED_TARGET)
    if(NOT is_alias)
        set_target_properties(SDL2::SDL2 PROPERTIES IMPORTED_GLOBAL TRUE)
    endif()
    target_sources(neogeorecomp PRIVATE src/window_sdl.c src/osd_menu.c)
    target_compile_definitions(neogeorecomp PRIVATE NG_HAVE_SDL)
    target_link_libraries(neogeorecomp PUBLIC SDL2::SDL2)

    # Optional OpenGL backend
    find_package(OpenGL QUIET)
    if(OpenGL_FOUND)
        target_compile_definitions(neogeorecomp PRIVATE NG_HAVE_OPENGL)
        if(APPLE)
            target_link_libraries(neogeorecomp PUBLIC "-framework OpenGL")
        else()
            target_link_libraries(neogeorecomp PUBLIC OpenGL::GL)
        endif()
        message(STATUS "neogeorecomp: OpenGL renderer enabled")
    endif()

    # Optional Vulkan backend
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        target_compile_definitions(neogeorecomp PRIVATE NG_HAVE_VULKAN)
        target_include_directories(neogeorecomp PRIVATE ${Vulkan_INCLUDE_DIRS})
        target_link_libraries(neogeorecomp PUBLIC Vulkan::Vulkan)
        message(STATUS "neogeorecomp: Vulkan renderer enabled")
    endif()
else()
    message(STATUS "neogeorecomp: SDL2 not found, building headless-only")
endif()
if(MSVC)
    target_compile_definitions(neogeorecomp PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(neogeorecomp PRIVATE /W3)
else()
    target_compile_options(neogeorecomp PRIVATE -O3 -Wall -Wextra)
endif()

# ---- m68krecomp: the recompiler, linked into each game's generator ----
add_library(m68krecomp_core STATIC
    tools/m68krecomp/decode.c
    tools/m68krecomp/emit.c
    tools/m68krecomp/recomp.c
    src/romload.c)
target_include_directories(m68krecomp_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/tools/m68krecomp)
target_link_libraries(m68krecomp_core PUBLIC musashi)
if(MSVC)
    target_compile_definitions(m68krecomp_core PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(m68krecomp_core PRIVATE /W3)
endif()

# The standalone tool, for flat images (used by the conformance harness).
add_executable(m68krecomp tools/m68krecomp/main.c)
target_link_libraries(m68krecomp PRIVATE m68krecomp_core)

target_include_directories(m68krecomp PRIVATE tools/m68krecomp)

# ---- conformance harness (docs/conformance.md) ----
# Musashi's 68000 test programs, recompiled at build time and run natively.
if(NEOGEORECOMP_BUILD_TESTS)
    enable_testing()
    file(GLOB CONF_BINS ${NG_EXT}/musashi/test/mc68000/*.bin)
    set(CONF_GEN ${CMAKE_CURRENT_BINARY_DIR}/conformance)
    file(MAKE_DIRECTORY ${CONF_GEN})
    set(conf_list "")
    set(conf_sources "")
    foreach(bin ${CONF_BINS})
        get_filename_component(name ${bin} NAME_WE)
        string(APPEND conf_list "T(${name})\n")
        set(outs ${CONF_GEN}/t_${name}_recomp_00.c ${CONF_GEN}/t_${name}_recomp_table.c)
        add_custom_command(
            OUTPUT ${outs}
            COMMAND m68krecomp --raw ${bin} --base 0x10000 --entry 0x10000
                    --prefix t_${name}_ --out ${CONF_GEN} --files 1
            DEPENDS m68krecomp ${bin}
            COMMENT "Recompiling conformance program ${name}")
        list(APPEND conf_sources ${outs})
    endforeach()
    file(CONFIGURE OUTPUT ${CONF_GEN}/conformance_list.h CONTENT "${conf_list}")
    add_executable(ng_conformance tests/conformance/harness.c src/cpu.c ${conf_sources})
    target_include_directories(ng_conformance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CONF_GEN})
    target_link_libraries(ng_conformance PRIVATE musashi)
    if(MSVC)
        target_compile_definitions(ng_conformance PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/BASELINE conf_baseline)
    string(STRIP "${conf_baseline}" conf_baseline)
    add_test(NAME conformance
             COMMAND ng_conformance --corpus ${NG_EXT}/musashi/test/mc68000 --min-pass ${conf_baseline})
endif()
