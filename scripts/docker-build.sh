#!/usr/bin/env bash
# Build the Poseidon backport inside the Amiga cross-toolchain container.
#
# No local m68k-amigaos toolchain is required: this runs the same public image CI
# uses (ghcr.io/rondoval/amiga-build-container, built on stefanreinauer/amiga-gcc
# with NDK 3.2), which ships the cross-compiler at /opt/m68k-amigaos, the MUI 5 and
# SANA-II SDKs (their paths exported as $MUI_INCLUDE_DIR / $SANA2_INCLUDE_DIR), and
# the `lha` archiver the `package` target needs.  The configure incantation and the
# image tag live HERE and nowhere else, so build.sh and CI stay in lock-step.
#
# Usage:
#   scripts/docker-build.sh                    # configure + build everything
#   scripts/docker-build.sh --target package   # ...stage + create the .lha (build first!)
#   scripts/docker-build.sh --target install   # ...stage the install() tree into the prefix
#   POSEIDON_CONFIGURE_ARGS="-DPOSEIDON_DEBUG_BACKEND=serial" scripts/docker-build.sh
#
# Any arguments are forwarded to `cmake --build <build dir>`.  The `package` target has
# no build dependency (it stages whatever is built), so package after a plain build:
#   scripts/docker-build.sh && scripts/docker-build.sh --target package
#
# Environment overrides:
#   POSEIDON_BUILD_IMAGE     Toolchain image tag (default: ghcr.io/rondoval/amiga-build-container:latest)
#   POSEIDON_CONFIGURE_ARGS  Extra args appended to the `cmake -S . -B <build dir>` configure step
#                            (e.g. -DPOSEIDON_DEBUG_BACKEND=... -DPOSEIDON_DEBUG_LEVEL=...)
#   POSEIDON_BUILD_DIR       CMake build directory, relative to the workspace (default: build)
#   POSEIDON_INSTALL_DIR     Install prefix (--target install), relative to the workspace
#                            (default: install)
set -euo pipefail

IMAGE=${POSEIDON_BUILD_IMAGE:-"ghcr.io/rondoval/amiga-build-container:latest"}
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ROOT=$(cd -- "${SCRIPT_DIR}/.." && pwd)

if ! command -v docker >/dev/null 2>&1; then
	echo "docker not found in PATH" >&2
	exit 1
fi

# Outputs are written back to the mounted workspace; -u keeps them host-owned (not
# root).  HOME=/tmp gives the arbitrary uid a writable home for tool caches; LC_ALL=C
# keeps the build locale-stable.  The MUI/SANA SDK paths and the toolchain (default
# /opt/m68k-amigaos) come from the image.  -DCMAKE_INSTALL_PREFIX pins the install
# tree into the mounted workspace so `--target install`/`package` can write it.
# Note: a build/ tree is tied to its prefix path (/work here) — do not share one build
# directory between docker and a native /opt/m68k-amigaos build; rm -rf it when switching.
docker run --rm \
	-v "${ROOT}:/work" \
	-w /work \
	-u "$(id -u):$(id -g)" \
	-e HOME=/tmp \
	-e LC_ALL=C \
	-e POSEIDON_CONFIGURE_ARGS \
	-e POSEIDON_BUILD_DIR \
	-e POSEIDON_INSTALL_DIR \
	"${IMAGE}" \
	sh -c 'BD=${POSEIDON_BUILD_DIR:-build}; ID=/work/${POSEIDON_INSTALL_DIR:-install}; cmake -S . -B "$BD" -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain.cmake -DCMAKE_INSTALL_PREFIX="$ID" -DMUI_INCLUDE_DIR="$MUI_INCLUDE_DIR" -DSANA2_INCLUDE_DIR="$SANA2_INCLUDE_DIR" ${POSEIDON_CONFIGURE_ARGS:-} && cmake --build "$BD" -j"$(nproc)" "$@"' \
	sh "$@"
