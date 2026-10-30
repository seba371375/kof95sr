cmake_minimum_required(VERSION 3.21)
project(kof95 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# Local copy of the neogeorecomp toolkit (see neogeorecomp/UPSTREAM.txt).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/neogeorecomp neogeorecomp_build)

# Your ROM files (MAME set "kof95" plus the "neogeo" system ROMs, unzipped).
# Without it the player builds interpreter-only.
set(KOF95_ROM_DIR "" CACHE PATH "Directory with your KOF95 and Neo Geo BIOS ROM files")
# Optional: entry points a previous run found at runtime (--dump-misses).
set(KOF95_ENTRIES "${CMAKE_BINARY_DIR}/entries.txt" CACHE FILEPATH "Extra entry points for the recompiler")

# Stage 1: the generator.
add_executable(kof95_recomp src/recomp_main.c src/game.c)
target_link_libraries(kof95_recomp PRIVATE m68krecomp_core)
target_include_directories(kof95_recomp PRIVATE src)

# Stage 2: the player, with the generated C when ROMs are available.
set(GAME_SOURCES src/main.c src/decomp/decomp.c)
set(GEN_DIR ${CMAKE_BINARY_DIR}/generated)
set(GEN_FILES 16)
if(KOF95_ROM_DIR)
    set(GEN_SOURCES ${GEN_DIR}/recomp_table.c)
    math(EXPR last "${GEN_FILES} - 1")
    foreach(i RANGE ${last})
        if(i LESS 10)
            list(APPEND GEN_SOURCES ${GEN_DIR}/recomp_0${i}.c)
        else()
            list(APPEND GEN_SOURCES ${GEN_DIR}/recomp_${i}.c)
        endif()
    endforeach()
    if(NOT EXISTS ${KOF95_ENTRIES})
        file(WRITE ${KOF95_ENTRIES} "")
    endif()
    file(MAKE_DIRECTORY ${GEN_DIR})
    if(CMAKE_CROSSCOMPILING)
        set(HOST_GEN_DIR "${CMAKE_SOURCE_DIR}/build/recomp/generated")
        set(HOST_RECOMP_BIN "${CMAKE_SOURCE_DIR}/build/recomp/kof95_recomp")
        if(EXISTS "${HOST_GEN_DIR}/recomp_table.c")
            add_custom_command(
                OUTPUT ${GEN_SOURCES} ${GEN_DIR}/recomp_common.h
                COMMAND ${CMAKE_COMMAND} -E copy_directory ${HOST_GEN_DIR} ${GEN_DIR}
                COMMENT "Using pregenerated KOF95 recompiled sources from host build")
        elseif(EXISTS "${HOST_RECOMP_BIN}")
            add_custom_command(
                OUTPUT ${GEN_SOURCES} ${GEN_DIR}/recomp_common.h
                COMMAND ${HOST_RECOMP_BIN} --rom-path ${KOF95_ROM_DIR} --out ${GEN_DIR} --files ${GEN_FILES}
                        --entries ${KOF95_ENTRIES}
                DEPENDS ${KOF95_ENTRIES}
                COMMENT "Recompiling KOF95 from ${KOF95_ROM_DIR} using host kof95_recomp")
        else()
            add_custom_command(
                OUTPUT ${GEN_SOURCES} ${GEN_DIR}/recomp_common.h
                COMMAND kof95_recomp --rom-path ${KOF95_ROM_DIR} --out ${GEN_DIR} --files ${GEN_FILES}
                        --entries ${KOF95_ENTRIES}
                DEPENDS ${KOF95_ENTRIES}
                COMMENT "Recompiling KOF95 from ${KOF95_ROM_DIR}")
        endif()
    else()
        add_custom_command(
            OUTPUT ${GEN_SOURCES} ${GEN_DIR}/recomp_common.h
            COMMAND kof95_recomp --rom-path ${KOF95_ROM_DIR} --out ${GEN_DIR} --files ${GEN_FILES}
                    --entries ${KOF95_ENTRIES}
            DEPENDS kof95_recomp ${KOF95_ENTRIES}
            COMMENT "Recompiling KOF95 from ${KOF95_ROM_DIR}")
    endif()
    list(APPEND GAME_SOURCES ${GEN_SOURCES})
endif()

add_executable(kof95 ${GAME_SOURCES} src/game.c)
target_link_libraries(kof95 PRIVATE neogeorecomp)
target_include_directories(kof95 PRIVATE src ${GEN_DIR})
if(KOF95_ROM_DIR)
    # Play scripted sessions and record entry points that only show up at
    # runtime (computed jumps), then the next build regenerates with them.
    add_custom_target(profile
        COMMAND kof95 --headless --rom-path ${KOF95_ROM_DIR} --frames 6000 --dump-misses ${KOF95_ENTRIES}
        COMMAND kof95 --headless --rom-path ${KOF95_ROM_DIR} --frames 12000
                --input ${CMAKE_CURRENT_SOURCE_DIR}/tests/coin_start.txt --dump-misses ${KOF95_ENTRIES}
        DEPENDS kof95
        COMMENT "Profiling scripted sessions for runtime-only entry points")
    target_compile_definitions(kof95 PRIVATE KOF95_HAVE_RECOMP)
    if(MSVC)
        set_source_files_properties(${GEN_SOURCES} PROPERTIES COMPILE_OPTIONS "/bigobj")
    else()
        # Use -O1 on generated 108 MB C tables to keep ARMv7 .text within the 32 MB branch limit
        # and avoid pathological GCC -O3 compile times on 20k+ generated functions.
        set_source_files_properties(${GEN_SOURCES} PROPERTIES COMPILE_OPTIONS "-O1")
    endif()
endif()
if(MSVC)
    target_compile_definitions(kof95 PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(kof95_recomp PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(kof95 PRIVATE -O3)
    if(MINGW)
        target_link_options(kof95 PRIVATE -static-libgcc -static-libstdc++ -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic)
    endif()
endif()

if(WIN32 AND TARGET SDL2::SDL2)
    add_custom_command(TARGET kof95 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:kof95> $<TARGET_FILE_DIR:kof95>
        COMMAND_EXPAND_LISTS)
endif()

# ---- PS Vita (.vpk packaging for VitaDB) ----
if(VITA)
    include("${VITASDK}/share/vita.cmake")
    set(VITA_APP_NAME "The King of Fighters '95")
    set(VITA_TITLEID  "KOF950001")
    set(VITA_VERSION  "01.00")
    # Enable extended main memory mode (+29 MB extra RAM) on PS Vita
    set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d ATTRIBUTE2=12")

    vita_create_self(kof95.self kof95 UNSAFE)
    vita_create_vpk(kof95.vpk ${VITA_TITLEID} kof95.self
        VERSION ${VITA_VERSION}
        NAME ${VITA_APP_NAME}
        FILE ${CMAKE_SOURCE_DIR}/vita/sce_sys/icon0.png sce_sys/icon0.png
        FILE ${CMAKE_SOURCE_DIR}/vita/sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
        FILE ${CMAKE_SOURCE_DIR}/vita/sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
        FILE ${CMAKE_SOURCE_DIR}/vita/sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
        FILE ${CMAKE_SOURCE_DIR}/kof95_controls.cfg kof95_controls.cfg
        FILE ${CMAKE_SOURCE_DIR}/kof95_dips.cfg kof95_dips.cfg
    )
endif()
