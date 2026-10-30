#!/usr/bin/env python3
"""
dump_symbols.py — Extract discovered entry points and routines from m68krecomp
into CSV and Ghidra labeling scripts.
"""

import sys
import os
import re

def main():
    table_path = sys.argv[1] if len(sys.argv) > 1 else "build/recomp/generated/recomp_table.c"
    out_csv = sys.argv[2] if len(sys.argv) > 2 else "re/symbols.csv"
    out_ghidra = sys.argv[3] if len(sys.argv) > 3 else "re/ghidra_import_symbols.py"

    if not os.path.exists(table_path):
        print(f"Error: {table_path} not found. Build recomp first.", file=sys.stderr)
        return

    pattern = re.compile(r'\{0x([0-9A-Fa-f]+)u,\s*([a-zA-Z0-9_]+)\}')
    entries = []

    with open(table_path, "r") as f:
        for line in f:
            m = pattern.search(line)
            if m:
                addr = int(m.group(1), 16)
                func = m.group(2)
                entries.append((addr, func))

    print(f"[dump_symbols] Found {len(entries)} entry points in {table_path}")

    # Write CSV
    os.makedirs(os.path.dirname(os.path.abspath(out_csv)), exist_ok=True)
    with open(out_csv, "w") as f:
        f.write("Address,Function,Region\n")
        for addr, func in entries:
            if addr < 0x100000:
                region = "CART_FIXED"
            elif addr < 0x300000:
                region = "CART_BANKED"
            elif addr >= 0xC00000:
                region = "BIOS"
            else:
                region = "OTHER"
            f.write(f"0x{addr:06X},{func},{region}\n")
    print(f"[dump_symbols] Wrote {out_csv}")

    # Write Ghidra Python Script
    with open(out_ghidra, "w") as f:
        f.write("# Ghidra script to label all discovered KOF95 entry points\n")
        f.write("# Run from Ghidra Script Manager or analyzeHeadless\n")
        f.write("from ghidra.program.model.symbol import SourceType\n\n")
        f.write("symbols = [\n")
        for addr, func in entries:
            f.write(f"    (0x{addr:06X}, '{func}'),\n")
        f.write("]\n\n")
        f.write("for addr_val, name in symbols:\n")
        f.write("    addr = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(addr_val)\n")
        f.write("    createLabel(addr, name, True, SourceType.USER_DEFINED)\n")
        f.write("    createFunction(addr, name)\n")
        f.write("print('Applied %d symbols to Ghidra project' % len(symbols))\n")
    print(f"[dump_symbols] Wrote Ghidra script: {out_ghidra}")

if __name__ == "__main__":
    main()
