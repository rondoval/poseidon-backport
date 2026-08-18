# CLAUDE.md

Guidance for coding agents working in this repository. `AGENTS.md` is a symlink to this file.

## What this repo is

**Poseidon for AmigaOS** — the 6.x line of the Poseidon USB stack, backported from the AROS 5.x
line to **AmigaOS 3.2** (m68k, GCC 16.1, NDK 3.2, cmake). The shipping version is `project(VERSION)` in the
top-level `CMakeLists.txt`, the single place it is written; every component reports it.)
It runs on a classic Amiga with PiStorm + RPi4/CM4 (Emu68) over the emu68 `xhci.device` — the two
repos are a **matched pair**: the driver lives in the `emu68-driver-stack` checkout under
`components/emu68-xhci-driver-context/`, and cross-ABI changes must land in both. (A frozen
`emu68-xhci-driver-legacy/` sibling there still ships the 5.x classic-ABI driver; don't edit it
for stack work.)

The AROS baseline SHA is in `AROS-BASELINE` and the verbatim extraction is tagged
`aros-extract-base`; original Poseidon 4.x sources are recoverable via
`git show <initial-import>:path`, with no AROS checkout needed.

## Build

```sh
./build.sh --build          # container build (image auto-pulled; no local toolchain needed)
./build.sh --package        # + build/Poseidon-<ver>-<cpu>.lha (use BACKEND=off for a release)
./build.sh --package --all-cpus   # one archive per released CPU (68020/68040/68060)
./build.sh --upload         # push binaries to a live Amiga via Cloanto AE.exe
./build.sh                  # = --build --upload (the edit-build-test loop)
```

Also `--tools` (upload the optional per-gadget tools) and `--dry-run` (upload: show, copy nothing).
Env knobs: `BACKEND=pistorm|serial|off` (debug sink, default `pistorm`), `DEBUG=<level>` (min
KPRINTF level, default 1 = verbose), `CPU=68020|68040|68060` (default `68040`; `FPU=` follows it,
soft for 020 and hard otherwise), `BUILD_IMAGE=`, `BUILD_DIR=`, `AE=`. A build tree is tied to one
CPU, so a non-default `CPU=` gets its own (`build-020/`, `build-060/`). `build.sh` wraps
`scripts/docker-build.sh`, which owns the docker invocation **and the toolchain image tag**
(`amiga-build-container:gcc-v16.1` — the same tag `emu68-driver-stack` builds on); CI runs the
same wrapper.

Every build ends with `scripts/check-regargs.py`, which fails the build if a function declaring
`asm("aN")` parameters was emitted with the stack calling convention. gcc 16.1 does that
**silently** when a prototype sees a parameter's struct as incomplete and the definition later
sees it complete, so keep such a struct complete before any prototype that names it.
`POSEIDON_SKIP_ABI_CHECK=1` skips the check.

Optimization is per tier, set in each target's `CMakeLists.txt`; everything else
(`-m$M68K_CPU -m$M68K_FPU-float -fomit-frame-pointer -mcrt=nix20 -Wno-array-bounds`) comes from
`cmake/toolchain.cmake`, and `-Wno-int-conversion` + the `aros_compat.h` force-include from one
`add_compile_options()` in the root `CMakeLists.txt`. Don't re-state any of those per target.
The CPU/FPU pair defaults to `68040`/`hard`; the release sweeps 68020-soft, 68040-hard and
68060-hard into one archive each, and `M68K_CPU` also lands in every `$VER` cookie
(`POSEIDON_CPU`) so an installed system says which variant it is.

| Tier | Targets | Flags |
|---|---|---|
| Hot path | `poseidon.library`, all 29 `*.class` | `-O3 -ffreestanding` |
| GUI | `Trident` | `-O2` |
| Cold | `c/` shell commands, `tools/` gadget tools, the embedded `poseidonusb` CAMD blob | `-Os` |

`-ffreestanding` must stay a **compile** option — as a link-only flag it is silently inert.

After editing a C file, build and confirm **zero errors and zero warnings** before reporting done.
There is no automated test suite; correctness is verified on the real Amiga.

## Layout

| Path | Contents |
|---|---|
| `poseidon.library/` | The stack core (`poseidon.library.c` ~10k lines + `poseidon_intern.h` + `poseidon.sfd` + romtag skeleton + the unbuilt `usbrom*startup.c`) |
| `classes/` | All `*.class` drivers (hub, hubss, hid, massstorage, audio, …; shared skeleton `class_main.c`, `common.h`) |
| `usbclass.library/` | Class-registry library — sfd + CMake only, no C |
| `include/` | Public headers: `libraries/poseidon.h`, `devices/usbhardware.h`, `libraries/usbclass.h` |
| `trident/` | The MUI preferences GUI |
| `c/`, `tools/` | CLI tools (PsdStackLoader, AddUSBHardware, …) and the optional per-gadget tools |
| `usbeject/` | USBEject — WBStartup daemon: Workbench "USB" menu, safe eject via `UCM_MSSafeEject` |
| `dist/`, `presets/` | Installer, icons, ReadMe template; shipped prefs |
| `docs/` | Architecture & ABI docs, porting playbook, implementation plan — **start at `docs/README.md`** |
| `scripts/` | `docker-build.sh` + the de-AROS porting scripts (`conf2sfd.py`, `dearos_lh.py`) |

## Open work

`docs/implementation-plan.md` is the **single open-work document** — everything in it is not yet
done, and nothing else tracks TODOs. When landing a phase, update the doc sections its
doc-maintenance map (§11) lists.

The lower-edge rework it grew out of is finished: the context HCD ABI ships and is the only client
ABI `xhci.device` speaks. Design: `docs/poseidon-context-hcd-abi.md`; rationale:
`docs/poseidon-vs-xhci-driver-model.md`.

## Hard constraints

- **The legacy HCD ABI is frozen.** `IOUsbHWReq` V1+V2 layout and all `UHCMD_*`/`UHIOERR_*`/
  `UHFB_*`/`UHCF_*` values are binary contract with classic third-party HCDs (Deneb, Subway, …).
  Never change these offsets or values. V3 may only append fields.
- **Do not regress the driver-workaround fixes (2026-07-02):** the class scan must not
  wire-switch alternates while probing (R5), and endpoint-recipient class requests must carry the
  full endpoint address (number | direction) in `wIndex` — the xhci.device workarounds that used
  to mask these were deleted.
- The address-0 enumeration lock is **`hub.class`-local** (`nh_Adr0Sema`, class-wide, held across
  the whole of `nConfigurePort`). The old stack-wide `PsdBase.ps_Adr0Sema` was retired:
  `hubss.class` is context-only and SuperSpeed has no default-address phase, so it doesn't
  serialize address 0 at all. Don't reintroduce a stack-wide semaphore, and don't narrow the scope
  of the hub.class one — a partial hold reintroduces the wrong-hub slot-binding race.

## Source conventions (de-AROS'd bebbo ABI)

- **LVO functions** are plain C with register annotations (`asm("d0")` …, libbase in `a6`) and a
  **parenthesised name** at the definition (`APTR (psdAllocVec)(…)`) so the inline call-macros
  don't expand there. Internal self-calls go through the sfdc inline stubs;
  `POSEIDON_BASE_NAME=ps` is set globally in `CMakeLists.txt`, so every call site needs an
  in-scope `ps` (classes often `#define ps <ctx>->Base` at file scope).
- **Adding public functions**: append to `poseidon.sfd` (bias 30, 6-byte stride) *and*
  `poseidon_funcs.inc` in the same order; headers are regenerated by sfdc at build time.
- **RawDoFmt rules** (OS3.2 exec): no `%p` (use `0x%08lx`); always `l`-size (`%ld`/`%lu`/`%lx`) —
  bare `%d` reads 16 bits but gcc varargs push 32.
- ROM-clean discipline: `__NOLIBBASE__`, `SysBase` from absolute `$4`
  (`EXEC_BASE_NAME (*(struct ExecBase **)4UL)`), string tables `const`.
- Match the surrounding (AROS-derived) code style in edits; the codebase predates C99 idioms.
- MUI 5 SDK's `__inline MUI_NewObject` is broken — it passes `&tags`, the address of the first
  named vararg, as the tag array, which is wrong at any optimization level. A force-included
  va_list replacement (`include/mui_newobject_fix.h`) shadows it; don't use the SDK inline.

## Documentation upkeep

`docs/porting-playbook.md` is the de-AROS recipe (genmodule→sfd, `AROS_LH`→C, MUI 5 idioms) — read
it before porting an AROS fix or adding a class driver.

The `docs/` architecture documents are reverse-engineered and kept current: if a change
invalidates a documented behavior (locking, enumeration order, scan semantics, ABI), update the
affected doc section in the same change. Line-number anchors in docs are indicative; function
names are the stable references.
