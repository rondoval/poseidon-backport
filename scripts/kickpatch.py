#!/usr/bin/env python3
"""Patch a 512 KiB AmigaOS Kickstart so exec scans the 2 MB image's extension bank.

Why
---
Emu68 maps a 2 MB Kickstart in four 512 KiB chunks: chunk 0 -> $E00000, chunk 1 ->
$A80000, chunk 2 -> $B00000 (so 1 and 2 are one contiguous megabyte at
$A80000..$B80000), chunk 3 -> $F80000.  Note $F00000 is *not* mapped in 2 MB mode.

Exec only looks for romtags where its scanBounds table tells it to, and a stock ROM
lists $F80000..$1000000 and $F00000..$F80000.  Neither covers $A80000, so modules
linked there would simply never be found.  Since $F00000 is unmapped anyway, its entry
is dead weight in a 2 MB image and is repurposed for the extension bank -- eight bytes,
no table growth, no re-pointing.

The table cannot be grown in place regardless: it is followed immediately by the
"chip memory" string.

Usage:  kickpatch.py <stock-kick.rom> <patched-kick.rom>
"""

import struct
import sys

KICK_BYTES = 512 * 1024
ROM_BASE = 0xF80000
ROM_HEADER = 0x11144EF9  # BIG_ROMS + JMP.  NOT $1111, which exec treats as a diag cart.

EXT_LOWER = 0x00A80000  # 2 MB image chunks 1+2
EXT_UPPER = 0x00B80000

# scanBounds as a stock ROM has it, from the entry the pointer at $F80560 names.
# The signature we match is the tail of it.
STOCK = (0x00F80000, 0x01000000, 0x00F00000, 0x00F80000, 0xFFFFFFFF)
PATCHED = (0x00F80000, 0x01000000, EXT_LOWER, EXT_UPPER, 0xFFFFFFFF)

SEARCH_LIMIT = 0x600  # the table lives just past the coldstart code; 0x400 in 47.115


def die(msg):
    sys.exit(f"kickpatch: {msg}")


def rl(buf, off):
    return struct.unpack(">I", buf[off:off + 4])[0]


def wl(buf, off, val):
    buf[off:off + 4] = struct.pack(">I", val)


def find_table(buf, want):
    """Offset of a 5-long run matching `want`, or None."""
    for off in range(0, SEARCH_LIMIT, 2):
        if all(rl(buf, off + 4 * i) == want[i] for i in range(len(want))):
            return off
    return None


def fix_checksum(buf):
    """Kickstart checksum: carry-wrapping sum of all longs must come to 0xffffffff.

    The checksum long itself lives at size-0x18 and is zeroed before summing.
    """
    size = len(buf)
    ck = size - 0x18
    wl(buf, ck, 0)
    total = 0
    for off in range(0, size, 4):
        total += rl(buf, off)
        if total > 0xFFFFFFFF:                 # end-around carry
            total = (total & 0xFFFFFFFF) + 1
    wl(buf, ck, (~total) & 0xFFFFFFFF)


def main(argv):
    if len(argv) != 3:
        die("usage: kickpatch.py <stock-kick.rom> <patched-kick.rom>")
    src, dst = argv[1], argv[2]

    buf = bytearray(open(src, "rb").read())
    if len(buf) != KICK_BYTES:
        die(f"{src} is {len(buf)} bytes, expected exactly {KICK_BYTES} (512 KiB)")
    if rl(buf, 0) != ROM_HEADER:
        die(f"{src} does not start with {ROM_HEADER:08x} — not a 512 KiB Kickstart image")

    if find_table(buf, PATCHED) is not None:
        die(f"{src} is already patched for the $A80000 extension bank")

    off = find_table(buf, STOCK)
    if off is None:
        die(f"scanBounds table not found in {src}. Expected the stock sequence "
            f"{' '.join(f'{v:08x}' for v in STOCK)} within the first {SEARCH_LIMIT:#x} "
            f"bytes; this ROM is either already modified or not a layout we know.")

    print(f"  scanBounds       ${ROM_BASE + off:06X} (file {off:#06x})")
    print(f"  before           " + " ".join(f"{rl(buf, off + 4 * i):08x}" for i in range(5)))

    wl(buf, off + 8, EXT_LOWER)
    wl(buf, off + 12, EXT_UPPER)

    print(f"  after            " + " ".join(f"{rl(buf, off + 4 * i):08x}" for i in range(5)))
    print(f"  scanned regions  "
          f"${rl(buf, off):06X}..${rl(buf, off + 4):07X}  "
          f"${rl(buf, off + 8):06X}..${rl(buf, off + 12):06X}")

    fix_checksum(buf)
    open(dst, "wb").write(bytes(buf))
    print(f"  checksum fixed, wrote {dst}")


if __name__ == "__main__":
    main(sys.argv)
