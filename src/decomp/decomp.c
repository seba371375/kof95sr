#include "decomp.h"
#include <stdio.h>

/*
 * Example decompiled routine:
 * Cartridge Header Entry Point USER ($000122)
 * Called by the Neo Geo MVS BIOS after boot check and RAM clearance.
 */
void decomp_user_init(void) {
    /*
     * 000122: USER entry point.
     * Original 68k sets up initial game state, interrupt masks, and vectors.
     */
}

static const kof95_decomp_hook_t s_decomp_hooks[1] = {
    {0, NULL, NULL}
};

const kof95_decomp_hook_t *kof95_get_decomp_hooks(size_t *count) {
    if (count) {
        *count = 0;
    }
    return s_decomp_hooks;
}
