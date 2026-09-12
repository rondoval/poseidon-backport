#!/usr/bin/env python3
"""
hcdpatch.py — point the ROM startup resident at a different host controller.

romstartup/usbromstart.c carries the HCD name in a fixed-size field tagged with its
own cookie (ROMSTART_HCD_COOKIE).  This overwrites that field, so a Kickstart image
can embed a controller other than the one the distribution was compiled for without
rebuilding the whole tree.

Usage:  scripts/hcdpatch.py <in-module> <out-module> <device-name>
"""

import sys

# Must match ROMSTART_HCD_COOKIE and sizeof(romstartHcd.name) in
# romstartup/usbromstart.c.  The trailing \x01 is the layout version: bump it there
# and here together if the field ever moves or changes size, so an old script cannot
# silently mis-patch a new module.
COOKIE = b"PSDHCD\x01\x00"
NAME_LEN = 32


def die(msg):
    sys.exit(f"hcdpatch: {msg}")


def main():
    if len(sys.argv) != 4:
        die("usage: hcdpatch.py <in-module> <out-module> <device-name>")

    src, dst, name = sys.argv[1], sys.argv[2], sys.argv[3]

    encoded = name.encode("latin-1", errors="strict")
    if not encoded:
        die("device name is empty")
    if len(encoded) + 1 > NAME_LEN:
        die(f"device name {name!r} is {len(encoded)} bytes; the slot holds "
            f"{NAME_LEN - 1} plus a NUL")

    buf = bytearray(open(src, "rb").read())
    off = buf.find(COOKIE)
    if off < 0:
        die(f"cookie {COOKIE!r} not found in {src} — is this the ROM startup "
            f"resident, and was it built from a source carrying the patchable slot?")

    # Whole field, NUL-padded: never leave a tail of the previous name behind.
    field = off + len(COOKIE)
    buf[field:field + NAME_LEN] = encoded + b"\x00" * (NAME_LEN - len(encoded))

    open(dst, "wb").write(buf)
    print(f"   patched HCD name -> {name} (slot at 0x{field:x})")


if __name__ == "__main__":
    main()
