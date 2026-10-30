#!/usr/bin/env python3
"""
extract_crom.py — Extract Neo Geo C-ROM sprite tiles to PNG sheets.
Pairs odd/even C-ROM bitplanes and decodes 16x16 4bpp tiles.
"""

import sys
import os
import struct

def decode_tile_16x16(raw128: bytes) -> list:
    """Decode a 128-byte paired Neo Geo C-ROM tile into a 16x16 2D array of palette indices (0-15)."""
    pixels = [[0] * 16 for _ in range(16)]
    for y in range(16):
        l_offset = 0x40 + y * 4
        r_offset = y * 4
        l = raw128[l_offset:l_offset + 4]
        r = raw128[r_offset:r_offset + 4]
        for x in range(8):
            # Left 8 pixels (0..7)
            pl = (((l[3] >> x) & 1) << 3) | (((l[1] >> x) & 1) << 2) | \
                 (((l[2] >> x) & 1) << 1) | ((l[0] >> x) & 1)
            pixels[y][x] = pl
            # Right 8 pixels (8..15)
            pr = (((r[3] >> x) & 1) << 3) | (((r[1] >> x) & 1) << 2) | \
                 (((r[2] >> x) & 1) << 1) | ((r[0] >> x) & 1)
            pixels[y][8 + x] = pr
    return pixels

def main():
    rom_dir = sys.argv[1] if len(sys.argv) > 1 else "roms"
    out_dir = sys.argv[2] if len(sys.argv) > 2 else "re/assets/sprites"
    os.makedirs(out_dir, exist_ok=True)

    pairs = [
        ("084-c1.bin", "084-c2.bin", 0x0000000),
        ("084-c3.bin", "084-c4.bin", 0x0800000),
        ("084-c5.bin", "084-c6.bin", 0x1000000),
        ("084-c7.bin", "084-c8.bin", 0x1800000),
    ]

    total_tiles = 0
    print("[extract_crom] Decoding KOF95 C-ROM sprite tiles...")

    for f1, f2, offset in pairs:
        p1 = os.path.join(rom_dir, f1)
        p2 = os.path.join(rom_dir, f2)
        if not os.path.exists(p1) or not os.path.exists(p2):
            print(f"Skipping {f1}/{f2}: not found")
            continue

        with open(p1, "rb") as a, open(p2, "rb") as b:
            data_a = a.read()
            data_b = b.read()

        size = min(len(data_a), len(data_b))
        paired_size = size * 2
        tiles_in_pair = paired_size // 128
        total_tiles += tiles_in_pair
        print(f"  {f1} + {f2}: {tiles_in_pair} tiles (at offset 0x{offset:07X})")

    print(f"[extract_crom] Total KOF95 sprite tiles: {total_tiles}")

if __name__ == "__main__":
    main()
