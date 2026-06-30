#!/usr/bin/env bash
#
# build.sh — build the Poseidon backport (in the shared GHCR toolchain container),
# package it as an installable .lha, and/or push the built binaries to a live Amiga
# over Cloanto Amiga Explorer (AE.exe).
#
# Pick any combination of --build / --package / --upload; with none of them given it
# does --build --upload (the usual edit-build-test loop).
#
# The upload pushes the freshly built binaries into the same locations the Installer
# uses; it does NOT install datatypes/icons/catalogs or touch S:User-Startup. For a
# first-time/full install use the .lha (--package) + Installer.
#
# Prerequisites
#   * --build / --package: docker (the toolchain image is public and auto-pulled).
#   * --upload: Amiga Explorer running on the Amiga (serial or TCP) with the matching
#     connection configured on the Windows side (this script drives AE.exe via WSL).
#
# Usage
#   ./build.sh [--build] [--package] [--upload] [--tools] [--dry-run]
#     --build     build the stack in the toolchain container (debug backend/level below)
#     --package   build, then create build/Poseidon-<ver>.lha (use BACKEND=off for a release)
#     --upload    push the built binaries to the Amiga
#     (none of --build/--package/--upload => --build --upload)
#     --tools     also upload the optional per-gadget tools to SYS:Tools/
#     --dry-run   upload: show what would be copied; copy nothing
#     -h, --help  this help
#
# Env overrides:
#     BUILD_IMAGE=<image>  override the toolchain container (default lives in scripts/docker-build.sh)
#     AE=<path to AE.exe>
#     BUILD_DIR=<path>     build dir, must live under the repo (default <repo>/build)
#     BACKEND=<pistorm|serial|off>  debug sink (default pistorm; use off for a release)
#     DEBUG=<level>                 min KPRINTF level (default 1 = verbose; higher = quieter)
#
set -euo pipefail

# --- config ------------------------------------------------------------------
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
BUILD_IMAGE="${BUILD_IMAGE:-}"             # empty => scripts/docker-build.sh owns the default tag
AE="${AE:-/mnt/c/Program Files/Cloanto/Amiga Explorer/Windows/AE.exe}"
DEBUG_LEVEL="${DEBUG:-1}"
DEBUG_BACKEND="${BACKEND:-pistorm}"

DO_BUILD=0 DO_PACKAGE=0 DO_UPLOAD=0 DO_TOOLS=0 DRY=0 EXPLICIT=0
for a in "$@"; do
    case "$a" in
        --build)   DO_BUILD=1;   EXPLICIT=1 ;;
        --package) DO_PACKAGE=1; EXPLICIT=1 ;;
        --upload)  DO_UPLOAD=1;  EXPLICIT=1 ;;
        --tools)   DO_TOOLS=1 ;;
        --dry-run) DRY=1 ;;
        -h|--help) sed -n '2,/^[^#]/{/^#/p}' "$0"; exit 0 ;;   # the leading comment block
        *) echo "unknown option: $a (try --help)" >&2; exit 2 ;;
    esac
done
# No action chosen => the usual build + upload loop.
if (( ! EXPLICIT )); then DO_BUILD=1; DO_UPLOAD=1; fi
# Packaging stages from a complete build, so it implies a build.
if (( DO_PACKAGE )); then DO_BUILD=1; fi

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

# --- build / package (delegated to the CI-coupled container wrapper) ----------
if (( DO_BUILD )); then
    # scripts/docker-build.sh owns the docker invocation, image tag and configure
    # incantation (toolchain file, MUI/SANA SDKs); we just feed it the backend knobs.
    case "$BUILD_DIR" in
        "$ROOT"/*) build_rel="${BUILD_DIR#"$ROOT"/}" ;;
        *) echo "BUILD_DIR must live under the repo ($ROOT) for the container build." >&2; exit 1 ;;
    esac
    export POSEIDON_CONFIGURE_ARGS="-DPOSEIDON_DEBUG_BACKEND=$DEBUG_BACKEND -DPOSEIDON_DEBUG_LEVEL=$DEBUG_LEVEL"
    export POSEIDON_BUILD_DIR="$build_rel"
    [[ -n "$BUILD_IMAGE" ]] && export POSEIDON_BUILD_IMAGE="$BUILD_IMAGE"

    what="building"; (( DO_PACKAGE )) && what="building + packaging"
    echo ">> $what via scripts/docker-build.sh (debug backend=$DEBUG_BACKEND, level=$DEBUG_LEVEL) ..."
    "$ROOT/scripts/docker-build.sh"
    if (( DO_PACKAGE )); then
        # package has no build dependency — stage the freshly built tree into the .lha.
        "$ROOT/scripts/docker-build.sh" --target package
        lha="$(ls -t "$BUILD_DIR"/Poseidon-*.lha 2>/dev/null | head -1 || true)"
        [[ -n "$lha" ]] && echo ">> package: $lha"
    fi
fi

# --- upload ------------------------------------------------------------------
if (( DO_UPLOAD )); then
    # --dry-run touches nothing, so it does not need AE.exe — it just previews the plan.
    if (( ! DRY )); then
        [[ -x "$AE" || -f "$AE" ]] || { echo "AE.exe not found at: $AE  (set AE=...)" >&2; exit 1; }

        # preflight: is the Amiga reachable? (retry — AE drops the odd request)
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

    echo ">> deploying core to LIBS: / SYS:Classes/USB / C: / SYS:Prefs ..."
    (( DRY )) || ensure_dir "SYS:Classes/USB"
    deploy_classes
    deploy_group "${CORE[@]}"

    if (( DO_TOOLS )); then
        echo ">> deploying per-gadget tools to SYS:Tools ..."
        (( DRY )) || ensure_dir "SYS:Tools"
        deploy_group "${GADGET_TOOLS[@]}"
    fi
fi

# --- summary -----------------------------------------------------------------
if (( DO_UPLOAD )); then
    echo
    if (( DRY )); then
        echo "dry run — nothing copied."
    else
        echo "done: $ok copied, $fail failed."
        echo "Note: poseidon.library / *.class are loaded into memory at boot — reboot"
        echo "      (or unload the stack and re-run PsdStackLoader / AddUSBClasses) to pick up changes."
    fi
fi
(( fail == 0 ))
