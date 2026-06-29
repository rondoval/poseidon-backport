#!/usr/bin/env bash
#
# deploy-amiga.sh — push the built Poseidon backport binaries to a live Amiga over
# Cloanto Amiga Explorer (AE.exe), into the same locations the Installer script uses.
#
# It does NOT install datatypes/icons/catalogs or touch S:User-Startup;
# for a first-time/full install use the CPack zip + Installer.
#
# Prerequisites
#   * Amiga Explorer running on the Amiga (serial or TCP) and the matching connection
#     configured on the Windows side (this script drives the Windows AE.exe via WSL).
#   * A build under build/  (run with --build, or `cmake --build build`, first).
#
# Usage
#   ./deploy-amiga.sh [--build] [--tools] [--dry-run]
#     --build     (re)configure + build before deploying (debug backend/level below)
#     --tools     also deploy the optional per-gadget tools to SYS:Tools/
#     --dry-run   show what would be copied; copy nothing
#     -h, --help  this help
#
# Env overrides:  
#     AE=<path to AE.exe>
#     BUILD_DIR=<path>
#     BACKEND=<pistorm|serial|off>  debug sink (default pistorm)
#     DEBUG=<level>                 min KPRINTF level (default 1 = verbose; higher = quieter)
#
set -euo pipefail

# --- config ------------------------------------------------------------------
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
AE="${AE:-/mnt/c/Program Files/Cloanto/Amiga Explorer/Windows/AE.exe}"
DEBUG_LEVEL="${DEBUG:-1}"
DEBUG_BACKEND="${BACKEND:-pistorm}"

DO_BUILD=0 DO_TOOLS=0 DRY=0
for a in "$@"; do
    case "$a" in
        --build)   DO_BUILD=1 ;;
        --tools)   DO_TOOLS=1 ;;
        --dry-run) DRY=1 ;;
        -h|--help) sed -n '2,/^[^#]/{/^#/p}' "$0"; exit 0 ;;   # the leading comment block
        *) echo "unknown option: $a (try --help)" >&2; exit 2 ;;
    esac
done

# Deploy table — "<source under build/>|<Amiga destination>".  Mirrors dist/Install:
CORE=(
    "poseidon.library/poseidon.library|LIBS:poseidon.library"
    "c/PsdStackLoader|C:PsdStackLoader"
    "c/AddUSBHardware|C:AddUSBHardware"
    "c/AddUSBClasses|C:AddUSBClasses"
    "c/PsdDevLister|C:PsdDevLister"
    "c/PsdErrorlog|C:PsdErrorlog"
    "trident/Trident|SYS:Prefs/Trident"
)
# Optional per-gadget tools
GADGET_TOOLS=(
    "tools/DRadioTool|SYS:Tools/DRadioTool"
    "tools/PencamTool|SYS:Tools/PencamTool"
    "tools/PowManTool|SYS:Tools/PowManTool"
    "tools/RocketTool|SYS:Tools/RocketTool"
    "tools/SonixcamTool|SYS:Tools/SonixcamTool"
    "tools/UPSTool|SYS:Tools/UPSTool"
)

# --- helpers -----------------------------------------------------------------
ae() { "$AE" "$@"; }                       # AE.exe always exits 0 — parse its output, not $?

# Create an Amiga dir if missing (MakeDir errors harmlessly when it already exists).
ensure_dir() { ae MakeDir "$1" >/dev/null 2>&1 || true; }

ok=0 fail=0
copy_one() {                               # copy_one <src-under-build> <amiga-dest>
    local src="$BUILD_DIR/$1" dest="$2" win out
    if [[ ! -f "$src" ]]; then
        printf '  \e[31mMISS\e[0m %-42s (not built — run --build?)\n' "$1" >&2
        fail=$((fail + 1)); return
    fi
    if (( DRY )); then
        printf '  would copy %-42s -> %s\n' "$1" "$dest"; return
    fi
    win="$(wslpath -w "$src")"
    out="$(ae Copy "$win" "$dest" /Y 2>&1 || true)"
    if printf '%s' "$out" | grep -qiE 'error|cannot|fail' || ! printf '%s' "$out" | grep -q '100%'; then
        printf '  \e[31mFAIL\e[0m %-42s -> %s\n' "$1" "$dest" >&2
        printf '       %s\n' "$(printf '%s' "$out" | tr -d '\r' | grep -iE 'error|cannot' | head -1)" >&2
        fail=$((fail + 1))
    else
        printf '  \e[32m OK \e[0m %-42s -> %s\n' "$1" "$dest"
        ok=$((ok + 1))
    fi
}

deploy_group() { local e; for e in "$@"; do copy_one "${e%%|*}" "${e#*|}"; done; }

# Every built class under build/classes/*/*.class -> SYS:Classes/USB/<file> (output names
# differ from dir names — pegasus.class, dm9601eth.class, usbaudio.class, … — so glob the
# actual artifacts rather than the source-dir names).
deploy_classes() {
    local f rel; local -a list
    mapfile -t list < <(find "$BUILD_DIR/classes" -name '*.class' -type f 2>/dev/null | sort)
    if (( ${#list[@]} == 0 )); then
        printf '  \e[31mMISS\e[0m %-42s (no classes built — run --build?)\n' "classes/*/*.class" >&2
        fail=$((fail + 1)); return
    fi
    for f in "${list[@]}"; do
        rel="${f#"$BUILD_DIR"/}"
        copy_one "$rel" "SYS:Classes/USB/$(basename "$f")"
    done
}

# --- optional build ----------------------------------------------------------
if (( DO_BUILD )); then
    echo ">> building (debug backend=$DEBUG_BACKEND, level=$DEBUG_LEVEL) ..."
    cmake -S "$ROOT" -B "$BUILD_DIR" \
        -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/toolchain.cmake" \
        -DPOSEIDON_DEBUG_BACKEND="$DEBUG_BACKEND" \
        -DPOSEIDON_DEBUG_LEVEL="$DEBUG_LEVEL" >/dev/null
    cmake --build "$BUILD_DIR" -j"$(nproc)"
fi

[[ -x "$AE" || -f "$AE" ]] || { echo "AE.exe not found at: $AE  (set AE=...)" >&2; exit 1; }

# --- preflight: is the Amiga reachable? (retry — AE drops the odd request) ----
if (( ! DRY )); then
    echo ">> checking Amiga Explorer connection ..."
    # NB: do NOT wrap AE.exe in `timeout` — under WSL Win32 interop that severs its
    # connection and it reports no volumes. AE has its own serial/TCP timeout anyway.
    connected=0
    for attempt in 1 2 3; do
        if ae Info 2>/dev/null | grep -q 'Volume:'; then connected=1; break; fi
        sleep 2
    done
    if (( ! connected )); then
        echo "No Amiga connection (AE Info returned no volumes after 3 tries)." >&2
        echo "Start Amiga Explorer on the Amiga and check the Windows-side connection." >&2
        exit 1
    fi
fi

# --- deploy ------------------------------------------------------------------
echo ">> deploying core to LIBS: / SYS:Classes/USB / C: / SYS:Prefs ..."
(( DRY )) || ensure_dir "SYS:Classes/USB"
deploy_classes
deploy_group "${CORE[@]}"

if (( DO_TOOLS )); then
    echo ">> deploying per-gadget tools to SYS:Tools ..."
    (( DRY )) || ensure_dir "SYS:Tools"
    deploy_group "${GADGET_TOOLS[@]}"
fi

# --- summary -----------------------------------------------------------------
echo
if (( DRY )); then
    echo "dry run — nothing copied."
else
    echo "done: $ok copied, $fail failed."
    echo "Note: poseidon.library / *.class are loaded into memory at boot — reboot"
    echo "      (or unload the stack and re-run PsdStackLoader / AddUSBClasses) to pick up changes."
fi
(( fail == 0 ))
