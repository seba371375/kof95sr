#ifndef KOF95_DECOMP_H
#define KOF95_DECOMP_H

#include <neogeorecomp/neogeo.h>
#include <neogeorecomp/cpu.h>
#include <neogeorecomp/bus.h>

/*
 * Decompilation hook registry:
 * Allows replacing any statically-recompiled 68000 routine with hand-written,
 * human-readable clean C.
 */

typedef struct {
    uint32_t   addr;
    const char *name;
    ng_func_t  fn;
} kof95_decomp_hook_t;

/* Returns table of active decompiled function hooks */
const kof95_decomp_hook_t *kof95_get_decomp_hooks(size_t *count);

#endif /* KOF95_DECOMP_H */
