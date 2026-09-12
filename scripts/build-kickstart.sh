#!/usr/bin/env bash
#
# build-kickstart.sh — link the ROM-resident part of the stack into a custom 2 MB
# Kickstart image, so USB comes up before strap picks a boot volume: mouse and keyboard
# in the early boot menu, and booting from a USB or NVMe drive.
#
# This is an advanced, entirely optional path. It does not replace, and does not touch, the
# normal filesystem installation — see ROM-ReadMe.md.
#
# Layout. Emu68 maps a 2 MB Kickstart in four 512 KiB chunks: chunk 0 -> $E00000,
# chunk 1 -> $A80000, chunk 2 -> $B00000 (1 and 2 being one contiguous megabyte), and
# chunk 3 -> $F80000. So the image is:
#
#     [ 512 KiB Kickstart mirror @ $E00000 ][ 1 MB extension @ $A80000 ][ Kickstart @ $F80000 ]
#
# Chunk 0 is not spare. While OVL is set — which it is at reset — Emu68 serves the whole
# first 512 KiB of the address space from its $E00000 shadow, so the reset vector itself
# (SSP and PC, longs 0 and 1) is fetched from chunk 0, not from $F80000. Emu68 does the
# same thing for a plain 512 KiB image, where it copies the Kickstart to both $E00000 and
# $F80000; a 2 MB image has to supply that mirror itself. $E00000 is not in scanBounds,
# so the mirror adds no romtags.
#
# A stock Kickstart does not scan $A80000, so kickpatch.py repoints one entry of its
# scanBounds table.
#
# Modules. The seven Poseidon ROM modules are found automatically — from the archive
# this script ships in (the ROM/ drawer of Poseidon-<ver>-<cpu>.lha, beside Libs/ and
# Classes/), or from the build tree when run out of a source checkout. Anything else is
# passed on the command line, so this stays driver-agnostic: on PiStorm/Emu68 that is
# normally bcmpcie.library + xhci.device, plus nvme.device if you want to boot from NVMe.
#
# Every module has to be ROM-clean — no writable data at all, because the bank is mapped
# read-only and writes to it vanish silently.
#
# Prerequisites
#   * Python 3, and romtool from amitools:  pipx install amitools
#   * Your own stock 512 KiB AmigaOS 3.2 Kickstart.
#
# Usage
#   build-kickstart.sh [--hcd <name>] <kick.rom> [module ...]
#     <kick.rom>   stock 512 KiB Kickstart (or set KICK=<path>)
#     module ...   extra ROM-able binaries to embed, e.g. your bcmpcie.library,
#                  xhci.device and nvme.device
#     --hcd <name> host controller the startup resident should add, e.g.
#                  --hcd myhci.device. Patches the name into a copy of the resident,
#                  so a different controller needs no rebuild of the distribution;
#                  without it the compiled-in default (cmake -DROMSTART_HCD) is used.
#                  It must name a real Poseidon host controller that is in the image:
#                  the resident opens it before DOS exists and waits on it with no
#                  timeout, so a name that opens something which is *not* a host
#                  controller hangs the boot rather than reporting anything.
#     -h, --help   this help
#
# Env overrides:
#     KICK=<path>       stock Kickstart, if not given as an argument
#     OUT=<path>        output image (default ./kick-usb-2m.rom from the archive,
#                       <BUILD_DIR>/kick-usb-2m.rom from a source tree)
#     BUILD_DIR=<path>  source tree only: build tree to take the Poseidon modules from
#                       (default <repo>/build)
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

KICK="${KICK:-}"
HCD=""
EXTRA=()
want_hcd=0
for a in "$@"; do
    if (( want_hcd )); then HCD="$a"; want_hcd=0; continue; fi
    case "$a" in
        -h|--help) sed -n '2,/^[^#]/{/^#/p}' "$0"; exit 0 ;;
        --hcd) want_hcd=1 ;;
        --hcd=*) HCD="${a#--hcd=}" ;;
        -*) echo "unknown option: $a (try --help)" >&2; exit 2 ;;
        *)  if [[ -z "$KICK" ]]; then KICK="$a"; else EXTRA+=("$a"); fi ;;
    esac
done
(( want_hcd == 0 )) || { echo "--hcd needs a device name (try --help)" >&2; exit 2; }

# --- where the Poseidon modules are -----------------------------------------------
# The resident sitting next to this script is what tells the two layouts apart: the
# archive ships it in ROM/, the source tree only ever has it in the build tree.
CLASSES=(hubss hub massstorage bootmouse bootkeyboard)
if [[ -f "$HERE/usbromstart" ]]; then
    KIT="$(dirname "$HERE")"
    ROMSTART="$HERE/usbromstart"
    POSEIDON_LIB="$KIT/Libs/poseidon.library"
    class_path() { echo "$KIT/Classes/USB/$1.class"; }
    OUT="${OUT:-$PWD/kick-usb-2m.rom}"
    MODULE_HINT="the archive is incomplete; unpack Poseidon-<ver>-<cpu>.lha again"
else
    BUILD_DIR="${BUILD_DIR:-$(dirname "$HERE")/build}"
    ROMSTART="$BUILD_DIR/romstartup/usbromstart"
    POSEIDON_LIB="$BUILD_DIR/poseidon.library/poseidon.library"
    class_path() { echo "$BUILD_DIR/classes/$1/$1.class"; }
    OUT="${OUT:-$BUILD_DIR/kick-usb-2m.rom}"
    MODULE_HINT="Poseidon modules come from \$BUILD_DIR — run ./build.sh --build"
fi

# In descending romtag priority so the scan at the end reads like the init sequence.
# Priorities live in the romtags, not in this order.
#   -44 poseidon.library   -45 the classes   -46 ROM startup
MODULES=("$POSEIDON_LIB")
for c in "${CLASSES[@]}"; do MODULES+=("$(class_path "$c")"); done
MODULES+=("$ROMSTART")

EXT_BASE=a80000                 # 2 MB chunks 1+2; NOT romtool's e00000 default
EXT_KIB=1024
OUT_BYTES=2097152

# The window every ROM module has to land in. Above ROM_BAND_HI the Emu68 module
# window does not exist yet; at or below ROM_BAND_LO the boot menu has already listed
# the volumes. See the priority table in docs/rom-image.md.
BAND_HI=-41                     # first free slot under romboot (-40)
BAND_LO=-49                     # last free slot above bootmenu (-50)

die() { echo "build-kickstart: $*" >&2; exit 1; }

# --- prerequisites ---------------------------------------------------------------
command -v python3 >/dev/null 2>&1 || die "python3 not found"
command -v romtool >/dev/null 2>&1 || die "romtool not found. Install amitools: pipx install amitools"
[[ -n "$KICK" ]] || die "no Kickstart given. Pass it as an argument or set KICK=<path> (try --help)"

MODULES+=("${EXTRA[@]}")

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
EXT="$TMP/poseidon-ext.rom"
PATCHED_KICK="$TMP/kick-patched.rom"

# --- 0. patch the Kickstart --------------------------------------------------------
# First, because it doubles as the validation of the input: size, ROM header and the
# scanBounds table it rewrites.
echo "== Patching Kickstart for the \$$EXT_BASE bank"
python3 "$HERE/kickpatch.py" "$KICK" "$PATCHED_KICK"

# --- 1. point the startup resident at a different host controller ------------------
# Patched into a copy, so the shipped resident stays what it was and a later run
# without --hcd still produces the stock image.
if [[ -n "$HCD" ]]; then
    # The resident opens this device pre-DOS and waits on it with no timeout, so a
    # name that resolves to something which is not a host controller hangs the boot
    # instead of failing. A name that is in the image is not proof it is an HCD, but
    # a name that is *not* is nearly always a typo or a forgotten module.
    hcd_seen=0
    for m in "${MODULES[@]}"; do
        [[ "$(basename "$m")" == "$HCD" ]] && hcd_seen=1
    done
    (( hcd_seen )) || echo "   WARNING: --hcd $HCD names no module being embedded — the resident will
            look for it at boot and find nothing. Fine only if it comes from your
            Kickstart or the board ROM; otherwise you forgot to pass the driver."

    PATCHED_ROMSTART="$TMP/usbromstart-hcd"
    python3 "$HERE/hcdpatch.py" "$ROMSTART" "$PATCHED_ROMSTART" "$HCD"
    for i in "${!MODULES[@]}"; do
        [[ "${MODULES[$i]}" == "$ROMSTART" ]] && MODULES[$i]="$PATCHED_ROMSTART"
    done
fi

# --- 2. the modules, and how much bank they take ------------------------------------
echo "== Modules"
mod_bytes=0
for m in "${MODULES[@]}"; do
    [[ -f "$m" ]] || die "missing module: $m
   ($MODULE_HINT; extra modules are the paths you passed.)"
    msize=$(stat -c%s "$m")
    mod_bytes=$(( mod_bytes + msize ))
    printf '   %-28s %7s bytes\n' "$(basename "$m")" "$msize"
done

# --- 3. what is already in the Kickstart -------------------------------------------
# The priorities everything here depends on, read off the real ROM rather than assumed.
# romboot (-40) is the one that matters most: it walks the ConfigDev chain, finds the
# romtag in the Emu68 board's diag area and SetCurrentBinding+InitResident's it, which
# is where the Emu68 module window (devicetree.resource, gic400.library,
# mailbox.resource, 68040.library) actually comes up. "diag init" (105) only relocates
# diag areas — it initialises nothing — so the whole ROM set has to sit *below* -40.
# bootmenu (-50) closes the window: the volumes we mount have to be listed by then.
echo "== Kickstart: $KICK"
if ! kick_scan="$(romtool scan -b f80000 "$KICK" 2>&1)"; then
    echo "$kick_scan" >&2
    die "romtool could not scan $KICK — is it a plain (unencrypted) 512 KiB image?"
fi

# Columns are separated by two or more spaces; single spaces occur *inside* a resident
# name, so split on runs of two rather than on whitespace.
SCAN_FS='  +'          # awk -F: type=$3, pri=$4, name=$5
kick_pri() { awk -F"$SCAN_FS" -v n="$1" '$5 == n { print $4 + 0; exit }' <<<"$kick_scan"; }

anchor() {                      # anchor <name> <expected pri>
    local got; got="$(kick_pri "$1")"
    if [[ -z "$got" ]]; then
        echo "   WARNING: $1 not found in the Kickstart resident list"
    elif [[ "$got" != "$2" ]]; then
        echo "   WARNING: $1 is priority $got, expected $2 — check the ROM module priorities"
    else
        printf '   %-22s pri %-5s ok\n' "$1" "$got"
    fi
}
anchor expansion.library    110
anchor "diag init"          105
anchor FileSystem.resource   80
anchor timer.device          50
anchor input.device          40
anchor romboot              -40
anchor bootmenu             -50
anchor strap                -60

clash="$(awk -F"$SCAN_FS" -v hi="$BAND_HI" -v lo="$BAND_LO" \
    '{ p = $4 + 0; if (p <= hi && p >= lo) print "   WARNING: " $5 " occupies priority " p " (our ROM band " hi ".." lo ")" }' <<<"$kick_scan")"
[[ -z "$clash" ]] || echo "$clash"

# --- 4. build the extension --------------------------------------------------------
echo "== Building extension ($EXT_KIB KiB @ \$$EXT_BASE)"
romtool build -o "$EXT" -t ext -s "$EXT_KIB" -e "$EXT_BASE" -f "${MODULES[@]}" >/dev/null

# --- 5. assemble -------------------------------------------------------------------
# romtool combine cannot do this: it only ever emits ext+kick, and rejects a 1024 KiB
# ext. The 2 MB layout needs the $E00000 Kickstart mirror in front — see the header:
# that mirror is where the reset vector is fetched from.
echo "== Assembling $OUT"
cat "$PATCHED_KICK" "$EXT" "$PATCHED_KICK" > "$OUT"
out_size=$(stat -c%s "$OUT")
(( out_size == OUT_BYTES )) || die "output is $out_size bytes, expected $OUT_BYTES (2 MB)"

# --- 6. verify and report ----------------------------------------------------------
romtool info "$PATCHED_KICK" | grep -qE '^is_kick +ok' \
    || die "the patched Kickstart no longer validates as a Kickstart image"

ext_scan="$(romtool scan -b "$EXT_BASE" "$EXT")"
found=$(wc -l <<<"$ext_scan")
(( found == ${#MODULES[@]} )) \
    || die "found $found residents in the extension, expected ${#MODULES[@]} — a module with no romtag?"

echo "== Residents in the extension (exec initialises these high priority first)"
awk -F"$SCAN_FS" '{ printf "   %-5s %-12s %-24s %s\n", $4, $3, $5, $6 }' <<<"$ext_scan"

# Below the band is always wrong: bootmenu (-50) has already listed the boot volumes by
# then, so a module that mounts one down there has missed it, and strap is right behind.
under="$(awk -F"$SCAN_FS" -v lo="$BAND_LO" \
    '{ p = $4 + 0; if (p < lo) printf "   %-24s priority %d\n", $5, p }' <<<"$ext_scan")"
[[ -z "$under" ]] || die "these modules sit below the ROM priority band (under $BAND_LO):
$under
   The boot menu (-50) lists the boot volumes before they run, and strap (-60) picks
   one straight after. See docs/rom-image.md."

# Above the band is a judgement call the script cannot make. Anything that touches the
# device tree, PCIe, MSI or the 68040 cache patches has to be under romboot (-40), which
# is where Emu68's own m68k modules are bound; a module that needs none of that -- a
# filesystem registering itself in FileSystem.resource, say -- is perfectly happy up
# there, and often has to be, so that it exists before anything mounts.
over="$(awk -F"$SCAN_FS" -v hi="$BAND_HI" \
    '{ p = $4 + 0; if (p > hi) printf "   %-24s priority %d\n", $5, p }' <<<"$ext_scan")"
if [[ -n "$over" ]]; then
    echo "   NOTE: these run before romboot (-40) opens the Emu68 module window:"
    echo "$over"
    echo "         Fine for a filesystem or anything else that only needs the Kickstart's"
    echo "         own resources; fatal for anything wanting devicetree.resource,"
    echo "         gic400.library, mailbox.resource or 68040.library's DMA cache patches."
fi

# How much of the bank is spoken for. RT_ENDSKIP of the last resident is the exact
# answer only when every module marks its own end (the Poseidon ones do, via
# classes/class_end.c) — a module whose romtag ends at the tag itself, as ODFileSystem's
# does, leaves the rest of its image uncounted. Fall back to the sum of the input files,
# which errs high by the hunk headers and relocation tables romtool strips.
used=$(( 0x$(awk -F"$SCAN_FS" 'END { print substr($2, 2) }' <<<"$ext_scan") ))
(( used >= mod_bytes )) || used=$mod_bytes
echo
echo "   extension  ~$used bytes of $((EXT_KIB * 1024)) ($(( (EXT_KIB * 1024 - used) / 1024 )) KiB free)"
echo "   image      $OUT ($out_size bytes)"

echo "Copy it to the SD FAT partition and point the initramfs line in config.txt at it."
echo "Keep the stock ROM beside it — rollback is a one-line edit. See ROM-ReadMe.md."
