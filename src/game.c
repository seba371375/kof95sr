/*
 * game.c - the King of Fighters '95 (NGM-084) ROM set, shared by the player
 * and the generator so both load the program identically.
 */
#include "game.h"

/* MAME set "kof95" (src/mame/snk/neogeo.cpp). File names follow the user's
 * dump (084-*.bin); the CRC32s match MAME:
 *   p1 2cba2716  s1 de716f8a  m1 6f2d7429
 *   v1 84861b56  v2 b38a2803  v3 d683a338
 *   c1 fe087e32  c2 07864e09  c3 a4e65d1b  c4 c1ace468
 *   c5 8a2c1edc  c6 f593ac35  c7 9904025f  c8 78eb0f9b
 *
 * The 2 MB P ROM holds the banked half first; the fixed $000000 half is its
 * second MB. Both are stored byte-swapped.
 *
 * The sprite region is 0x1A00000 bytes with a hole at 0x1400000-0x17FFFFF:
 * c7/c8 load at 0x1800000, hence the explicit crom_offsets. */
const ng_game_t k_kof95 = {
    .name = "kof95",
    .title = "The King of Fighters '95",
    .prom_size = 0x200000,
    .prom = {
        {"084-p1.bin", 0x100000, 0x000000, 0x100000, 1},
        {"084-p1.bin", 0x000000, 0x100000, 0x100000, 1},
    },
    .crom_size = 0x1A00000,
    .crom_pairs = {
        {"084-c1.bin", "084-c2.bin"},
        {"084-c3.bin", "084-c4.bin"},
        {"084-c5.bin", "084-c6.bin"},
        {"084-c7.bin", "084-c8.bin"},
    },
    .crom_offsets = {0x0000000, 0x0800000, 0x1000000, 0x1800000},
    .srom = "084-s1.bin",
    .m1 = "084-m1.bin",
    .vrom_size = 0x1000000,
    .vrom = {"084-v1.bin", "084-v2.bin", "084-v3.bin"},
    .vrom_offsets = {0x000000, 0x400000, 0x800000},
};
