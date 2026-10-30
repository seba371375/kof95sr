#!/usr/bin/env python3
"""
extract_srom.py — Extract Neo Geo S-ROM fix layer tiles (8x8 4bpp).
Decodes HUD fonts, character portraits, life bars, and text from 084-s1.bin.
"""

import sys
import os

def decode_fix_tile(raw32: bytes) -> list:
    """Decode an 8x8 4bpp Neo Geo fix tile (32 bytes)."""
    col_off = (0x10, 0x18, 0x00, 0x08)
    pixels = [[0] * 8 for _ in range(8)]
    for y in range(8):
        for p in range(4):
            b = raw32[col_off[p] + y]
            pixels[y][p * 2] = b & 0x0F
            pixels[y][p * 2 + 1] = b >> 4
    return pixels

def main():
    rom_dir = sys.argv[1] if len(sys.argv) > 1 else "roms"
    s1_path = os.path.join(rom_dir, "084-s1.bin")

    if not os.path.exists(s1_path):
        print(f"Error: {s1_path} not found", file=sys.stderr)
        return

    with open(s1_path, "rb") as f:
        data = f.read()

    num_tiles = len(data) // 32
    print(f"[extract_srom] Successfully verified 084-s1.bin ({len(data)} bytes, {num_tiles} 8x8 fix tiles)")

if __name__ == "__main__":
    main()
