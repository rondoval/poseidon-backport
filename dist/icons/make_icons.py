#!/usr/bin/env python3
"""Build every committed .info icon from its PNG + .info.src descriptor.

Each icon gets a faithful OS3.5 ColorIcon (full colour + transparency)
plus a planar fallback, honouring TYPE/STACK/DEFAULTTOOL from its .info.src.
Uses icontool fork (--import-icon + --import-coloricon + --set-defaulttool).

The PNGs are committed source art; this script only matters when regenerating
the .info files (themselves committed static assets, so building the stack
needs nothing here).

Requirements (host-side only):
    * python3 with pypng
    * icontool with --import-coloricon / --set-defaulttool

Usage:   ICONTOOL=/path/to/icontool/icontool python3 make_icons.py
"""
import os
import subprocess
import sys
import tempfile

from amiga_icon_template import write_template

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
ICONTOOL = os.environ.get("ICONTOOL",
                          os.path.normpath(os.path.join(ROOT, "..", "icontool", "icontool")))

# (source PNG, .info.src, output .info) — paths relative to the repo root
ICONS = [
    ("dist/icons/installer.png", "dist/icons/installer.info.src", "dist/Install.info"),
    ("dist/icons/Trident.png", "dist/icons/Trident.info.src", "dist/Trident.info"),
    ("dist/icons/def_PSD.png", "dist/icons/def_PSD.info.src", "dist/def_PSD.info"),
]


def parse_info_src(path):
    meta = {}
    if path and os.path.exists(path):
        with open(path, encoding="latin-1") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                k, v = line.split("=", 1)
                meta[k.strip().upper()] = v.strip()
    return meta


def convert(png_rel, src_rel, out_rel):
    png = os.path.join(ROOT, png_rel)
    out = os.path.join(ROOT, out_rel)
    meta = parse_info_src(os.path.join(ROOT, src_rel))
    itype = meta.get("TYPE", "TOOL")
    stack = int(meta.get("STACK", "4096"))
    deftool = meta.get("DEFAULTTOOL")

    with tempfile.NamedTemporaryFile(suffix=".info", delete=False) as tf:
        template = tf.name
    try:
        write_template(template, itype, stack)
        cmd = [sys.executable, ICONTOOL,
               "--import-icon", png,           # classic planar fallback
               "--import-coloricon", png]       # OS3.5 ColorIcon (full colour)
        if deftool:
            cmd += ["--set-defaulttool", deftool]
        cmd += [template, "-o", out]
        subprocess.run(cmd, check=True)
    finally:
        os.unlink(template)
    print(f"wrote {out_rel}  (type={itype} stack={stack}"
          + (f" defaulttool={deftool}" if deftool else "") + ")")


def main():
    if not os.path.exists(ICONTOOL):
        sys.exit(f"icontool not found at {ICONTOOL} (set $ICONTOOL)")
    for png_rel, src_rel, out_rel in ICONS:
        convert(png_rel, src_rel, out_rel)


if __name__ == "__main__":
    main()
