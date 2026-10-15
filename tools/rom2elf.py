#!/usr/bin/env python3
"""
rom2elf.py — Convert Neo Geo P-ROM + BIOS to a valid 68000 ELF32 executable.
Allows disassembly and decompilation in Ghidra, MCP rea, IDA, radare2, etc.
"""

import struct
import sys
import os

# ELF Constants
EI_MAG0, EI_MAG1, EI_MAG2, EI_MAG3 = 0x7F, ord('E'), ord('L'), ord('F')
ELFCLASS32 = 1
ELFDATA2MSB = 2      # Big Endian (Motorola 68000)
EV_CURRENT = 1
ET_EXEC = 2          # Executable
EM_68K = 4           # Motorola 68000
PT_LOAD = 1
PF_R = 4
PF_W = 2
PF_X = 1

def swap16(data: bytes) -> bytes:
    """Byte-swap 16-bit words (little-endian ROM dump -> big-endian 68000 bus)."""
    a = bytearray(data)
    for i in range(0, len(a) - 1, 2):
        a[i], a[i + 1] = a[i + 1], a[i]
    return bytes(a)

def build_elf(rom_dir: str, out_path: str):
    p1_path = os.path.join(rom_dir, "084-p1.bin")
    bios_path = os.path.join(rom_dir, "sp-s2.sp1")

    if not os.path.exists(p1_path):
        print(f"Error: {p1_path} not found", file=sys.stderr)
        sys.exit(1)

    with open(p1_path, "rb") as f:
        p1_raw = f.read()

    # KOF95 P-ROM is 2 MB:
    # 0x000000-0x0FFFFF in file is banked area (placed at bus 0x200000)
    # 0x100000-0x1FFFFF in file is fixed area (placed at bus 0x000000)
    prom_fixed = swap16(p1_raw[0x100000:0x200000])
    prom_banked = swap16(p1_raw[0x000000:0x100000])

    bios_data = b""
    if os.path.exists(bios_path):
        with open(bios_path, "rb") as f:
            bios_raw = f.read()
        bios_data = swap16(bios_raw[:0x20000])
        print(f"[rom2elf] Loaded BIOS ({len(bios_data)} bytes) from {bios_path}")

    # Segments to create:
    # 1. Fixed P-ROM: VA 0x000000, size 0x100000
    # 2. Banked P-ROM: VA 0x200000, size 0x100000
    # 3. BIOS: VA 0xC00000, size 0x020000 (if present)
    num_phdrs = 3 if bios_data else 2

    # Vector table in Neo Geo:
    # SP at 0x000000, Reset PC at 0x000004
    entry_point = struct.unpack(">I", prom_fixed[4:8])[0] if len(prom_fixed) >= 8 else 0x100

    ehdr_size = 52
    phdr_size = 32
    phdrs_total = num_phdrs * phdr_size

    # Data offsets aligned to 0x1000
    p_offset_fixed = 0x1000
    p_offset_banked = p_offset_fixed + len(prom_fixed)
    p_offset_bios = p_offset_banked + len(prom_banked)

    # Build ELF Header
    e_ident = bytearray(16)
    e_ident[0:4] = bytes([EI_MAG0, EI_MAG1, EI_MAG2, EI_MAG3])
    e_ident[4] = ELFCLASS32
    e_ident[5] = ELFDATA2MSB
    e_ident[6] = EV_CURRENT
    e_ident[7] = 0  # ELFOSABI_NONE

    ehdr = struct.pack(
        ">16sHHIIIIIHHHHHH",
        bytes(e_ident),
        ET_EXEC,
        EM_68K,
        EV_CURRENT,
        entry_point,
        ehdr_size,          # e_phoff (immediately after ehdr)
        0,                  # e_shoff (no section headers for bare binary)
        0,                  # e_flags
        ehdr_size,
        phdr_size,
        num_phdrs,
        0,                  # e_shentsize
        0,                  # e_shnum
        0                   # e_shstrndx
    )

    # Program Headers
    # 1: Fixed P-ROM
    ph_fixed = struct.pack(
        ">IIIIIIII",
        PT_LOAD,
        p_offset_fixed,
        0x000000,          # p_vaddr
        0x000000,          # p_paddr
        len(prom_fixed),    # p_filesz
        len(prom_fixed),    # p_memsz
        PF_R | PF_X,        # p_flags
        0x1000              # p_align
    )

    # 2: Banked P-ROM
    ph_banked = struct.pack(
        ">IIIIIIII",
        PT_LOAD,
        p_offset_banked,
        0x200000,          # p_vaddr
        0x200000,          # p_paddr
        len(prom_banked),   # p_filesz
        len(prom_banked),   # p_memsz
        PF_R | PF_X,        # p_flags
        0x1000
    )

    ph_bios = b""
    if bios_data:
        ph_bios = struct.pack(
            ">IIIIIIII",
            PT_LOAD,
            p_offset_bios,
            0xC00000,          # p_vaddr
            0xC00000,          # p_paddr
            len(bios_data),     # p_filesz
            len(bios_data),     # p_memsz
            PF_R | PF_X,
            0x1000
        )

    # Write file
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(ehdr)
        f.write(ph_fixed)
        f.write(ph_banked)
        if bios_data:
            f.write(ph_bios)
        
        # Pad to p_offset_fixed
        cur = f.tell()
        if cur < p_offset_fixed:
            f.write(b"\x00" * (p_offset_fixed - cur))
        
        f.write(prom_fixed)
        f.write(prom_banked)
        if bios_data:
            f.write(bios_data)

    print(f"[rom2elf] Successfully created {out_path}")
    print(f"  Fixed ROM:  0x000000 - 0x{len(prom_fixed):06X} ({len(prom_fixed)//1024} KB)")
    print(f"  Banked ROM: 0x200000 - 0x{0x200000+len(prom_banked):06X} ({len(prom_banked)//1024} KB)")
    if bios_data:
        print(f"  BIOS ROM:   0xC00000 - 0x{0xC00000+len(bios_data):06X} ({len(bios_data)//1024} KB)")
    print(f"  Entry PC:   0x{entry_point:06X}")

if __name__ == "__main__":
    rom_dir = sys.argv[1] if len(sys.argv) > 1 else "roms"
    out_path = sys.argv[2] if len(sys.argv) > 2 else "re/kof95.elf"
    build_elf(rom_dir, out_path)
