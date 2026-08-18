#!/usr/bin/env bash
# Build the Poseidon backport inside the Amiga cross-toolchain container.
#
# No local m68k-amigaos toolchain is required: this runs the same public image CI
# uses (ghcr.io/rondoval/amiga-build-container, built on stefanreinauer/amiga-gcc
# with GCC 16.1 + NDK 3.2 — the same tag emu68-driver-stack builds on), which ships
# the cross-compiler at /opt/m68k-amigaos, the MUI 5 and
# SANA-II SDKs (their paths exported as $MUI_INCLUDE_DIR / $SANA2_INCLUDE_DIR), and
# the `lha` archiver the `package` target needs.  The configure incantation and the
# image tag live HERE and nowhere else, so build.sh and CI stay in lock-step.
#
# Usage:
#   scripts/docker-build.sh                    # configure + build everything
#   scripts/docker-build.sh --target package   # ...stage + create the .lha (build first!)
#   scripts/docker-build.sh --target install   # ...stage the install() tree into the prefix
#   POSEIDON_CONFIGURE_ARGS="-DPOSEIDON_DEBUG_BACKEND=serial" scripts/docker-build.sh
#   # a CPU variant (the release builds 68020/soft, 68040/hard and 68060/hard):
#   POSEIDON_BUILD_DIR=build-060 \
#   POSEIDON_CONFIGURE_ARGS="-DM68K_CPU=68060 -DM68K_FPU=hard" scripts/docker-build.sh
#
# Any arguments are forwarded to `cmake --build <build dir>`.  The `package` target has
# no build dependency (it stages whatever is built), so package after a plain build:
#   scripts/docker-build.sh && scripts/docker-build.sh --target package
#
# Environment overrides:
#   POSEIDON_BUILD_IMAGE     Toolchain image tag (default: ghcr.io/rondoval/amiga-build-container:gcc-v16.1)
#   POSEIDON_CONFIGURE_ARGS  Extra args appended to the `cmake -S . -B <build dir>` configure step
#                            (e.g. -DPOSEIDON_DEBUG_BACKEND=... -DPOSEIDON_DEBUG_LEVEL=...,
#                            -DM68K_CPU=... -DM68K_FPU=...)
#   POSEIDON_BUILD_DIR       CMake build directory, relative to the workspace (default: build)
#   POSEIDON_INSTALL_DIR     Install prefix (--target install), relative to the workspace
#                            (default: install)
#   POSEIDON_SKIP_ABI_CHECK  Set to 1 to skip the post-build register-argument check
#                            (scripts/check-regargs.py); see that script for what it catches.
#   POSEIDON_SKIP_MUI38_CHECK
#                            Set to 1 to skip the MUI 3.8 subset check
#                            (scripts/check-mui38.py), which keeps the GUI fleet runnable
#                            on muimaster.library 19; see that script for what it catches.
set -euo pipefail

IMAGE=${POSEIDON_BUILD_IMAGE:-"ghcr.io/rondoval/amiga-build-container:gcc-v16.1"}
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ROOT=$(cd -- "${SCRIPT_DIR}/.." && pwd)

if ! command -v docker >/dev/null 2>&1; then
	echo "docker not found in PATH" >&2
	exit 1
fi

# Must contain no single quote.  `sh -ec` runs it, so the first failing step aborts the build;
# "$@" is whatever the caller passed us, forwarded to `cmake --build`.
BUILD_RECIPE='
BD=${POSEIDON_BUILD_DIR:-build}
ID=/work/${POSEIDON_INSTALL_DIR:-install}

cmake -S . -B "$BD" \
	-DCMAKE_TOOLCHAIN_FILE=cmake/toolchain.cmake \
	-DCMAKE_INSTALL_PREFIX="$ID" \
	-DMUI_INCLUDE_DIR="$MUI_INCLUDE_DIR" \
	-DSANA2_INCLUDE_DIR="$SANA2_INCLUDE_DIR" \
	${POSEIDON_CONFIGURE_ARGS:-}

cmake --build "$BD" -j"$(nproc)" "$@"

# Post-build gates; each fails the build on its own.  See the scripts for what they catch.
[ "${POSEIDON_SKIP_ABI_CHECK:-0}" = 1 ]   || python3 scripts/check-regargs.py "$BD"
[ "${POSEIDON_SKIP_MUI38_CHECK:-0}" = 1 ] || python3 scripts/check-mui38.py
'

# Outputs are written back to the mounted workspace; -u keeps them host-owned (not
# root).  HOME=/tmp gives the arbitrary uid a writable home for tool caches; LC_ALL=C
# keeps the build locale-stable.  The MUI/SANA SDK paths and the toolchain (default
# /opt/m68k-amigaos) come from the image.  -DCMAKE_INSTALL_PREFIX pins the install
# tree into the mounted workspace so `--target install`/`package` can write it.
# Note: a build/ tree is tied to its prefix path (/work here) — do not share one build
# directory between docker and a native /opt/m68k-amigaos build; rm -rf it when switching.
# The trailing `sh` is $0 for the recipe, so "$@" inside it starts at our first argument.
docker run --rm \
	-v "${ROOT}:/work" \
	-w /work \
	-u "$(id -u):$(id -g)" \
	-e HOME=/tmp \
	-e LC_ALL=C \
	-e POSEIDON_CONFIGURE_ARGS \
	-e POSEIDON_BUILD_DIR \
	-e POSEIDON_INSTALL_DIR \
	-e POSEIDON_SKIP_ABI_CHECK \
	-e POSEIDON_SKIP_MUI38_CHECK \
	"${IMAGE}" \
	sh -ec "${BUILD_RECIPE}" sh "$@"
