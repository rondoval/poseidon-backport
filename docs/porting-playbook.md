# Porting playbook (de-AROS → AmigaOS 3.2)

How to port a Poseidon component (genmodule / AROS) into a clean NDK 3.2 / bebbo-gcc / cmake one.
The C *logic* ports almost untouched; the work is replacing the genmodule calling-convention glue
(§1) and supplying the AROS vocabulary (§1.5). The mechanical parts are automated by two scripts in
`scripts/`: **`conf2sfd.py`** and **`dearos_lh.py`**.

The original port is complete — all 29 class drivers, `poseidon.library`, the CLI tools and Trident
are built and running. This playbook is kept for the work that still uses the recipe: porting an
AROS fix into a component, adding a new class driver, or touching the `.sfd`/`sfdc` flow.

---

## 0. Scope — what was taken and what was dropped

Source: the AROS `rom/usb/` tree, recorded in `AROS-BASELINE`; the verbatim extraction is tagged
`aros-extract-base`.

**Taken:** `poseidon.library` (the stack core), `usbclass.library` (the base meta-class every class
inherits — a headers-only cmake target, since nothing opens it), all class drivers, Trident, and
the CLI tools.

**Dropped, and why:**

* **`pciusb` (EHCI/OHCI/UHCI), `pcixhci` (AROS xHCI), `vusbhc` (hosted-AROS virtual HC)** — this
  port drives a host controller through a `.device` (`xhci.device`), so no hardware drivers are
  needed. The lower-edge contract is specified in
  [poseidon-context-hcd-abi.md](poseidon-context-hcd-abi.md).
* **`felsunxi`** (Allwinner FEL) and **`guiapps/ps3eye`** (camera demo) — irrelevant on metal.
* **`trident/catalogs`** — the *submodule link* was dropped; every translation was copied into the
  tree.

**Kept but unported:** the `usbromstartup` resource (`poseidon.library/usbromearlystartup.c`,
`usbromlatestartup.c`) — ROM-resident early/late autostart. The sources are still in the tree and
are not compiled; they still carry AROS-isms and reference the dropped HW drivers. A resident
stack loader supersedes them (see the ROM-ability phase in the implementation plan).

---

## 1. Per-component de-AROS recipe

### The checklist

1. **`conf2sfd.py <c>.conf --modname <m> --base _<M>Base` → `<m>.sfd`** (§1.1).
2. **`dearos_lh.py <c>.c --inplace`** (and the internal `.h` if it has `AROS_UF*`/`AROS_LH`) (§1.2).
3. Write **`<m>_main.c`** (romtag + `funcTable[]` from `conf2sfd --mode functable`) + **`<m>_end.c`** (§1.3).
4. Remove genmodule/`<aros/*>` includes from the component header; set the base-var convention (§1.4).
5. Handle the component-specific AROS-isms (§2): stubs, OOP/HIDD, hooks, `ADD2INIT`, debug.
6. Write the component `CMakeLists.txt` (§1.6); **build clean — 0 warnings** before declaring done.

`aros_compat.h` (§1.5) is force-included into everything and grows as new AROS-isms surface.

### 1.1 genmodule `.conf` → `.sfd` + `sfdc`

`conf2sfd.py` emits the `.sfd`: config header (`==base`/`==basetype`/`==libname`/`==bias 30`), the
`functionlist` body **verbatim** (same `name(args)(REGS)` syntax), the cdef vararg stubs as
`==varargs` entries, and **drops the trailing `##begin class` (HIDD) blocks**. `sfdc --addvectors
none --target m68k-amigaos --mode {clib,macros,pragmas,proto}` then generates the headers.

**Key rule:** the component includes **`<inline/<m>.h>`**, *not* `<proto/<m>.h>` — the latter also
pulls the plain `clib` prototypes which **conflict** with our register-arg LVO definitions. External
libraries it merely *calls* (e.g. poseidon→usbclass) use `<proto/…>` normally.

### 1.2 `AROS_LH` / `AROS_UFH` / `AROS_UFP` → plain C (`dearos_lh.py`)

```c
AROS_LH1(APTR, psdAllocVec, AROS_LHA(ULONG,size,D0), LIBBASETYPEPTR, ps, 5, psd) {INIT …EXIT}
 →  APTR (psdAllocVec)(ULONG size asm("d0"), struct PsdBase *ps asm("a6")) { … }
```
- Register letters come from the spec; libbase last in `a6`.
- **The LVO function name is parenthesised** `(psdAllocVec)(…)` so the same-named *inline call macro*
  (used for the library's own internal calls) does not expand at the definition.
- `AROS_UFH`/`AROS_UFP` (hook callbacks / their prototypes) → same, but **no `a6`**.
- Also drops `AROS_LIBFUNC_INIT/EXIT`, `ADD2{INIT,OPEN,EXPUNGE}LIB`; `GM_UNIQUENAME(x)→x`;
  `LIBBASETYPEPTR → struct <Base> *`.
- **Internal calls are left untouched** — `psdAllocVec(size)` expands the inline macro, which jsr's
  the LVO with the libbase in `a6` (§1.4).
- **Gotcha:** the script reads/writes **latin-1** — some AROS sources are ISO-8859 (a UTF-8 read
  silently skips them, leaving the file un-transformed).
- The regex handles only LH/UFH/UFP. `AROS_LD`/`AROS_LDA` (library-descriptor protos), `AROS_LC*`
  (self-calls), `AROS_INTH*`/`AROS_INTFUNC_*` (soft-int handlers) must be hand-converted (§3.3).

### 1.3 Library skeleton — `*_main.c` + `*_end.c`

Hand-written (template: `gic400_main.c` / `bcmpcie.library`). **No trampolines.**
- `struct Resident` romtag (`RTC_MATCHWORD`, `RTF_AUTOINIT`, `NT_LIBRARY`, `residentpri`).
- `funcTable[]` = `LibOpen, LibClose, LibExpunge, LibNull,` then **the LVO functions in `.sfd`
  order** (generated `*_funcs.inc`), then `(APTR)-1`. Order **is** the ABI.
- `initTable = { sizeof(struct <Base>), funcTable, 0, LibInit }`.
- `LibInit/Open/Close/Expunge` wrap the component's kept `libInit/libOpen/libExpunge` hooks
  (un-`static` them so `*_main.c` can call them). `doNotExecute` is the link entry point.
- **`SysBase`** defined here (the de-AROS'd `.c` references it `extern`).
- **Seglist lives in the libbase struct** (e.g. add `BPTR ps_SegList;`), **never a `static`** — a
  writable static breaks ROM-ability (see the ROM-ability phase in
  [implementation-plan.md](implementation-plan.md)).

### 1.4 Base-variable & `__NOLIBBASE__`

Compile every component `-D__NOLIBBASE__` so no proto auto-declares a base global; the component
supplies each base itself:
- **Its own API** (internal calls): `#define <M>_BASE_NAME ps` before `<inline/<m>.h>` — `ps` is the
  libbase parameter present in every LVO function. So `psdAllocVec(size)` → LVO via `a6 = ps`.
- **System libs** it opens: `#define DOSBase ps->ps_DosBase` etc.; `SysBase` is the real global.
- **Other Poseidon libs** it calls (usbclass): the existing `#define UsbClsBase puc->puc_ClassBase`
  satisfies `USBCLASS_BASE_NAME`.
- **Clients** (classes/tools) use their *own* base var (`ps` + `__<M>_NOLIBBASE__`, or
  `#define <M>_BASE_NAME <theirvar>`). The `.sfd` `==base` is `_<M>Base` (house convention) —
  reconcile client base-var names when porting them.

### 1.5 `aros_compat.h` — the AROS-vocabulary shim

`include/aros_compat.h`, **force-included** (`-include`) into every TU, supplies what the NDK lacks
(so the de-AROS'd sources need no per-file type edits). Currently:
- Types: `IPTR`, `SIPTR`, `RAWARG`; `_sfdc_vararg` = `APTR` (vararg string-literals are `void*`, not a
  wide-char error); `VOID_FUNC` (`typedef void (*VOID_FUNC)();`); a self-contained inline `stricmp`
  (libc `strcasecmp` drags in `malloc.o` → unresolved `SysBase` in the freestanding link).
- Byte order: `AROS_*2BE`=identity (m68k is BE), `AROS_*2LE`/`AROS_LE2*`=`__builtin_bswap` (USB is LE).
- List macros: `ForeachNode`/`ForeachNodeSafe`, `NEWLIST`.
- Misc: `MEMF_SEM_PROTECTED`=0, `PKCTRL_IPTR`=`PKCTRL_LONG`, `AROS_WORSTALIGN`, `AROS_STACKSIZE`,
  `AROS_SLOWSTACKFORMAT_{PRE,ARG,POST}` (m68k stack-varargs pointer idiom, `ARG = &x+1`).

Extend it (guarded with `#ifndef`) whenever a new component trips over another AROS-ism.

**Not all AROS-isms can be a passive shim** — two need real code changes:
- **`NP_UserData`** is an AROS/OS4 `CreateNewProc` tag that **OS 3.2 dos.library silently ignores**,
  so `tc_UserData` stayed unset and the spawned task read a garbage base → illegal-instruction crash.
  No shim; `psdSpawnSubTask` sets `tc_UserData` by hand under `Forbid()` after `CreateNewProcTags`.
  (A `#define NP_UserData …` would have *hidden* the bug — it compiles fine, just does nothing.)
- **`%p` in `RawDoFmt`** — classic OS 3.2 exec `RawDoFmt` has no `%p` (it's an AROS extension). Every
  Poseidon formatter routes through exec `RawDoFmt` (`psdSafeRawDoFmt`, `psdCopyStrFmt`,
  `psdAddErrorMsg`), so `%p` yields garbage/blank output. **Convert `%p` → `0x%08lx`**; for the **MUI
  text-engine image escape** use **`\33O[%08lx]`** (no `0x` prefix — MUI parses the bracket as hex).
  When porting a component, `grep` its `.c` for `%p` and convert the live (non-`//`) sites.

### 1.6 Build & link (cmake)

Flags live at exactly one of three levels — a new component adds **only** its `-O` level.

- **Toolchain-wide** (`cmake/toolchain.cmake`, byte-identical to `emu68-driver-stack`'s):
  `-m$M68K_CPU -m$M68K_FPU-float -fomit-frame-pointer -mcrt=nix20 -Wno-array-bounds`. Never
  repeat these per target. The CPU/FPU pair defaults to `68040`/`hard`; the release builds the
  same tree three times (68020-soft, 68040-hard, 68060-hard), one archive each, so never assume
  a specific CPU in code — and there is currently no CPU-conditional source in the tree.
  `-Wno-array-bounds` silences GCC ≥ 12's false positive on the `EXEC_BASE_NAME`
  absolute-`$4` idiom (it fires at each call site, so a pragma can't scope it).
- **Tree-wide** (root `CMakeLists.txt` `add_compile_options()`): `-Wno-int-conversion` and the
  `-include include/aros_compat.h` force-include.
  - `-Wno-int-conversion` is **approved and load-bearing**: m68k tag/vararg calls inherently mix
    pointers and ULONGs through sfdc's vararg array (pointers are ULONG-sized here; gcc ignores
    `__attribute__((iptr))`), and GCC 14+ makes int↔pointer conversion an *error* by default.
  - Use the `"SHELL:-include …"` form for any *second* force-include (Trident's
    `mui_newobject_fix.h`); CMake de-dups a bare repeated `-include` flag.
- **Per target:** the `-O` level, plus `-ffreestanding` for libraries/classes and
  `-D__NOLIBBASE__`. `-O3` for `poseidon.library` and the classes, `-O2` for Trident, `-Os` for
  the `c/` + `tools/` shell programs. Include dirs = `include/`, the component, the generated
  sfd dir.
  - **`-ffreestanding` is a COMPILE option, not a link option** — as a link-only flag it is
    silently inert (GCC 16.1 migration finding, shared with `emu68-driver-stack`).
- **Link freestanding:** `-nostdlib -nostartfiles -s -Wl,-e,_doNotExecute`, libs
  `-Wl,--start-group -lc -lgcc -Wl,--end-group` (`-lc` libnix string fns; `-lgcc` intrinsics —
  grouped so `__divsi3` resolves). **`-ldebug` is not hardcoded** — added per target by
  `psd_debug_finalize()` **only** for the `serial` debug backend (§5).
- `OUTPUT_NAME <m>` + `SUFFIX ".library"`. Reuse `cmake/GenerateSfdHeaders.cmake`.

---

## 2. Component-specific AROS-isms

| AROS-ism | Where | Replacement |
|---|---|---|
| `mmakefile.src` | every dir | per-component `CMakeLists.txt`. |
| MUI GUI | boot-class config dialogs, `popo.gui.c`, Trident | real MUI via **MUI 5.0** + a file-scope MUI-base accessor (no global) + `-lamiga` — see §4. |
| `debug.h` / `KPRINTF` / `XPRINTF` / `DB` | every component | the shared **`include/debug.h`** switchable backend (§5); call sites unchanged. AROS `bug()`/`D()` map to `KPRINTF` when ported. |
| OOP / HIDD (`<oop/oop.h>`, `<hidd/*>`) | **`hid` class only**, behind `#if __AROS__` | the blocks have no native `#else`, so bebbo **auto-excludes** them; use the original `input.device` path (compare `bootmouse.class.c`). |
| `AROS_UFH/UFP` hooks (BOOPSI dispatchers) | Trident (×many), shellapps | handled by `dearos_lh.py`; or SDI hook headers. |
| `(HOOKFUNC)func` cast on `h_Entry` | hook assignments (shellapps, Trident) | gcc errors `-Wincompatible-pointer-types` via the *typedef* even though `HOOKFUNC`≡`ULONG(*)()`; cast to the literal `(ULONG (*)(void))` (or `(APTR)`) instead. |
| NDK inlines typed `RET (*)()` (`SetFunction`, `RawDoFmt`, `Interrupt.is_Code`) | patches, formatters, interrupt servers | GCC 15+ defaults to **C23**, where `()` means `(void)` — a prototyped function (ours all carry `asm("dN")` register args) no longer converts implicitly and it is a hard **error**, not a warning. Cast explicitly, spelled `(ULONG (*)(void))` / `(void (*)(void))`: correct in both C11 and C23. An `APTR`-typed variable still passes silently (GCC's `void*`↔function-pointer extension) — that is why only *some* call sites break. |
| `ADD2INIT/EXIT` linker sets | Trident `locale.c` | explicit init/cleanup in `main()`. |
| `bootloader.resource` `usbdebug` arg | `libInit` | dropped (debug is compile-time). |
| `PSF_KLOG` boot-arg | error-log path | removed — framework is purely compile-time (§5). |

---

## 3. Class drivers

Each class is a full genmodule library, so §1 runs **per class** — but the shared skeleton + GUI/ROM
infrastructure make most *mechanical*. No per-class `.sfd` (the 3 usbclass ABI vectors come from
`usbclass_headers` + the shared `class_main.c` skeleton). The installer drops any new `.class` into
`SYS:Classes/USB/`, so there is **no packaging work per class**. (`felsunxi` — Allwinner FEL boot
protocol — was deleted: irrelevant on Amiga, the one exception to "keep all sources".)

### 3.1 Per-class recipe (mechanical core)

1. **`dearos_lh.py --inplace`** the `<name>.class.c` *and* its `.h`; **`%p` sweep** (§1.5).
2. **CMakeLists** via `add_poseidon_class()` (§3.2): set `CLASS_NAME`/`VERSION`/`REVISION`/`PRI`,
   `HAS_LIBOPEN`/`HAS_LIBCLOSE` as the source needs; link `poseidon_headers` (+ `mui_headers` if GUI,
   + family deps from §3.4).
3. **GUI classes:** the class struct header ends with the `mui_base.h` block (`MUI_BASE_USERDATA` +
   `MUI_BASE_FIELD` + `#include "mui_base.h"`) — that include also pulls the `MUI_NewObject` fix (§4).
4. **Start ROM-clean** (the ROM-ability phase in
   [implementation-plan.md](implementation-plan.md)): the `$4` exec base + `const` tables are
   baked into `class_main.c`/`common.h`; don't introduce a new writable global.
5. **New `aros_compat.h` vocab / non-NDK headers** carried into `include/` as they surface.
6. **Build 0/0**; deploy adds it to the install automatically.

### 3.2 `add_poseidon_class()` CMake helper

Hoisted into the top-level `CMakeLists.txt` (next to `generate_sfd_headers`); collapses a class's
~40-line CMakeLists to ~3 lines. Signature:
`add_poseidon_class(<name> BASETYPE <NepXxxBase> VERSION <v> REVISION <r> PRI <p> [INCLUDE <h>] [GUI]
[AMIGALIB] [SOURCES …] [DEFINES HAS_LIBOPEN HAS_LIBCLOSE …] [DEPS sana2_headers …])`.
`GUI` adds `mui_headers` + `-lamiga`; `DEPS` carries family header targets.

Supporting INTERFACE targets: **`sana2_headers`** (`SANA2_INCLUDE_DIR` defaults under
`AMIGA_SDK_ROOT`=`$HOME/amiga`, i.e. `…/NDK3.2R4/SANA+RoadshowTCP-IP/include` — no literal home path
in the build; SANA-II is *not* in bebbo's default include) and **`mui_headers`** (the MUI 5 SDK, §4).

Header availability: **AHI** sub-driver headers (`libraries/ahi_sub.h`, `defines/`, `inline/`,
`proto/`, `devices/ahi.h`) **are in the bebbo toolchain** → audio needs no vendoring. **CAMD**
(`midi/camd.h`/`libraries/camd.h`) is absent but the full SDK isn't needed (§3.4). serial, `input.device`,
`usbparallel` are all in the NDK.

### 3.3 Class skeleton & embedded-device recipe

The shared `class_main.c` + `common.h` bake in the **`$4` exec base** and `const` tables. Romtag name
need not equal dir name (pass the *romtag* name to `add_poseidon_class` so `<name>.class.c`/`.h`
resolve; the dir is only the `add_subdirectory` path — e.g. dir `pegasuseth` → `pegasus.class`, dir
`davicometh` → `dm9601eth.class`, dir `camdmidi` → `camdusbmidi.class`).

**Embedded-device (dual-library) recipe** (massstorage / serial / eth / audio / arosx): the class
`MakeLibrary`'s a second library/device in `libInit`.
- dearos with **two** `--libbasetype`s: the class base for `<name>.class.c`/`.h`, the device base for
  `dev.c` (its `a6` is the *device* base, not the class base). When both bases appear in one file,
  disambiguate by basevar and re-type only the funcs whose `a6` carries the second base.
- `dev.h` uses `AROS_LD`/`AROS_LDA` (library-descriptor protos) which `dearos_lh.py` does **not**
  handle → hand-convert to plain `asm()` protos (template `massstorage/dev.h`). The one `AROS_LC1`
  self-call → a direct call; `&AROS_SLIB_ENTRY(devX, dev, n)` in `DevFuncTable[]` → `(APTR) devX`.
- un-`static` `libInit`/`libExpunge` (skeleton calls them via `extern`); add `BPTR nh_SegList;` to the
  class base; replace any global `SysBase` value-use → `EXEC_BASE_NAME`.
- **`bug()`** (AROS raw debug) → `KPRINTF(10, (…))`; fix `%d`→`%ld` (classic `RawDoFmt` `%d` reads 16
  bits; args are 32-bit).
- A zero-init `const` sentinel lands in `.bss`; an explicit `= {0}` initializer keeps it in `.rodata`
  so `.bss`=0.

**Class extra vectors** (generic, gated): `class_main.c` supports `-DCLASS_VECTORS_HDR="…"` to pull a
class header that declares externs and `#define CLASS_EXTRA_VECTORS` (the funcTable tail: reserved
`LibNull`s + the extra vectors). Absent for every other class ⇒ their binaries stay byte-identical.
Used by camdmidi (§3.4).

**Definition of done (per class):** builds 0/0; ROM-clean (`.bss`=0, no new named writable globals —
`nm` check); `%p`-free debug strings; GUI (if any) opens via Trident's Classes panel; deployed
`.class` binds its device on plug.

### 3.4 Per-family notes

- **Serial (`cdcacm`/`serialpl2303`/`serialcp210x`).** Dual library: class `NepSerialBase` + embedded
  `usbmodem.device` via `dev.c`/`devInit`. The canonical embedded-device pattern (§3.3). Each binds
  its USB-serial chipset → a `usbmodem.device` unit.
- **Ethernet / SANA-II (×8: cdceth, asixeth, rndis, pegasus[dir pegasuseth], dm9601eth[dir
  davicometh], moschipeth, ethwrap, lan78xx).** Each a SANA-II `.device` (Roadshow/AmiTCP bind),
  dual-library (`NepEthBase` class + `NepEthDevBase` device, `IOSana2Req`, `DEPS sana2_headers`).
  - **GUI base is per-instance** (`NepClassEth`/`ncp_MUIBase`, GUI subtask `tc_UserData`=`ncp`) — the
    `mui_base.h` block uses `NepClassEth`. `rndis`/`lan78xx` have no GUI dialog but their *headers*
    still pull `<libraries/mui.h>`, so add `DEPS … mui_headers` (no `-lamiga`).
  - **Struct-header location varies**: most keep `NepEthBase` in `<name>.h` (default `INCLUDE`);
    `cdceth` and `lan78xx` use `<name>.class.h` (pass `INCLUDE`).
  - `cdceth` needs carried AROS headers `hardware/cdc/cdc_{eem,ncm}.h`. `bluetooth`/`stir4200` carry
    `bluetooth/*.h`, `irda/irlap.h`, `devices/{bluetoothhci,irda}.h`. `palmpda` is SANA-II (PPP),
    serial-style (`NepSerialBase`/`NepSerDevBase`).
  - AROS `CALLHOOKPKT` → `CallHookPkt` (utility.library) needs a local `nh = ncp->ncp_ClsBase;` for
    `UtilityBase` at the call site. Bounce-buffering is **not** the class's job (xhci.device bounces).
- **Input (`egalaxtouch`/`simplemidi`/`arosx`).** Feed `input.device` like bootmouse (no HIDD).
  `egalaxtouch` per-instance MUI base (`NepClassHid`/`nch_MUIBase`), coordinate calibration;
  `simplemidi` libbase base (`NepHidBase`/`nh_MUIBase`). `arosx` (hard): a dual library — the class
  (`AROSXClassBase`) embeds a full **`arosx.library`** (`AROSXBase`, XInput event API) `MakeLibrary`'d
  in `libInit`; its `lib*` hooks **renamed `ax*`** to avoid colliding with the class skeleton's;
  generated `proto/arosx.h` (sfd) for the GUI; per-instance MUI base via `MUI_BASE_FIELD =
  arosxb->MUIBase`; old amiga.lib `CreatePort`/`CreateExtIO` → exec `CreateMsgPort`/`CreateIORequest`;
  maps XInput → `IECLASS_*`.
- **`audio` (AHI sub-driver, `usbaudio.class`).** Dual library: the class `NepAudioBase` MakeLibrary's
  an embedded **AHI sub-driver** `NepAudioSubLibBase` (`usbaudio.audio`, 19 `subLib*` vectors) from
  `SubLibFuncTable[]` in `libInit`. AHI library/inline/`ahi_sub` headers ship in the toolchain;
  `ahi.device` opened **per-binding** at runtime (`AHIBase = nch->nch_AHIBase`). Wrinkles `dearos_lh.py`
  doesn't cover (scripted in `port_audio.py`): **dual basetype in one file** — re-type the `subLib*`
  funcs (basevar `nas`) `struct NepAudioBase * nas asm("a6")` → `struct NepAudioSubLibBase *` (class
  funcs use `nh`); **`SUBLIBBASETYPEPTR` is a substring of `LIBBASETYPEPTR`** so a blind
  `replace("LIBBASETYPEPTR",…)` corrupts it — resolve it → `struct NepAudioSubLibBase *` and drop its
  `#define` *before* the transform; **`AROS_LD`/`AROS_LDA`** + **`AROS_LC1`** hand-converted,
  `&AROS_SLIB_ENTRY(x,nep,n)` → `(APTR) x`; **`AROS_INTH1`** (3 `Cause()`'d soft-int players) →
  `ULONG name(type var asm("a1"))` (`is_Data` in A1); RTIso `h_Entry` casts → `(APTR)`; drop the stray
  `;` before `{` in the `subLibOpen`/`Close` defs.
- **CAMD MIDI (`camdmidi` → romtag `camdusbmidi.class`).** A simplemidi clone: single library, libbase
  MUI base (`NepHidBase`/`nh_MUIBase`). **No CAMD SDK** — only a small vendored
  `include/midi/camddevices.h` (`struct MidiDeviceData` + `MDD_Magic`, self-contained, no AROS
  `<libcore/compiler.h>`). The bind-time CAMD driver written to `DEVS:Midi/<id>` is **compiled from
  `camd/poseidonusb.c`** (not a frozen blob):
  - **The class exports 2 extra library vectors** the driver calls — `usbCAMDOpenPort`@−90,
    `usbCAMDClosePort`@−96 (after `.skip 7`) — via the generic `CLASS_VECTORS_HDR` extension point
    (§3.3; camd's is `camd_vectors.h`). Without this the driver jumps to a dead vector.
  - **Driver build pipeline** (class CMakeLists): `camdusbmidi.sfd` → `sfdc` → `proto/camdusbmidi.h`
    (inline LP-stubs through global `nh`, `#define CAMDUSBMIDI_BASE_NAME nh`); `poseidonusb.c` carries a
    `doNotExecute` entry stub (`return -1` → `moveq #-1,d0;rts`) in section `.camdhdr` and pins its
    `MidiDeviceData` to `.camdmagic`, linked with `camd/camddriver.ld` (from `amiga.xe`;
    `KEEP(.camdhdr) KEEP(.camdmagic)` first) → a CAMD-shaped HUNK exe with the stub at offset 0 and
    **`MidiDeviceData`/`MDD_Magic` at offset 4** (where camd.library reads it). `camd/gen_camddriver.py`
    turns the exe into a `static const ULONG CAMDDriver[]` **plus a recompile-safe
    `CAMDDriver_NAMEOFFSET`** (per-unit name patch), asserting the stub+`'MDEV'` layout. *(Build quirk:
    pulls `camdusbmidi.h`'s GUI `mui_base.h` block as dead code → needs `-Iclasses` + a throwaway
    `-DEXEC_BASE_NAME=SysBase`; the tiny binary drops it.)* **Not hardware-verified** — the offset-4
    placement rests on the ldscript + section attrs; needs a real CAMD app + USB-MIDI device to confirm.
- **`hid` (full USB HID → `input.device`).** The dreaded OOP/HIDD rewrite never materialised: the
  `<oop/oop.h>`/`HIDD` code is two `#if defined(__AROS__)` blocks with no native `#else`, so bebbo
  drops them and the **Hodges `input.device` path is the always-compiled default** — no OOP. A large
  but mechanical de-AROS (`port_hid.py`: dearos all 5 files). hid-specific bits:
  - **lowlevel.library joyport patches.** hid exports `nReadJoyPort`/`nSetJoyPortAttrsA` and
    `SetFunction`s them over lowlevel.library vectors −5/−22 for USB gamepads. These are `AROS_LH` with
    a **second basetype** (`a6` = `struct Library * LowLevelBase`, not the hid base — disambiguate by
    basevar like audio's `nas`); `AROS_SLIB_ENTRY(name, hid, n)` → `name` (patch address). The patched
    fns recover the hid base **without a global** via `FindName(&EXEC_BASE_NAME->LibList, libname)`
    (ROM-clean), and call the saved original through **`AROS_CALL1/2`** → a typed reg-arg
    function-pointer cast (`conv_call`). `AROS_LD` protos in `hid.class.h` → `conv_ld`. Carried
    `libraries/lowlevel_ext.h`.
  - **Two MUI GUIs** (`hid.gui.c` config + `hidctrl.gui.c` per-binding control), both per-instance MUI
    base (`NepClassHid`/`nch_MUIBase`) — one `mui_base.h` block in `hid.h` covers both.
  - Fix-ups: `GM_UNIQUENAME(nAllocHid())` nested-call form (paren-matching strip);
    `ShutdownA(SD_ACTION_COLDREBOOT)` → `ColdReboot()` (NDK 3.2 has no dos.library `Shutdown`; the
    USB-keyboard Ctrl-Alt-Del reset path); `<dos/dostags.h>` for `SYS_*`/`NP_StackSize`.
- **Long tail:** `dfu` needed a varargs `psdDoHubMethod` added to `poseidon.sfd` (only the `A` form was
  there), no `libOpen`. `ptp` → `stricmp` inline (§1.5). `hubss` is an AROS-*native* rewrite (not
  Hodges): restructured `<aros/debug.h>`/`proto/arossupport.h`/`LC_LIBDEFS_FILE`/`GM_UNIQUENAME`/
  `bug`→KPRINTF; renamed `hubss_class.{c,h}` → `hubss.class.{c,h}`; mapped baseless
  `LibFindTagItem`/`LibNextTagItem` → utility.library `FindTagItem`/`NextTagItem` (opened a
  `UtilityBase`); added the missing `libExpunge`.

---

## 4. MUI 5 GUIs

Trident and the class config GUIs were written for AROS Zune (~MUI 3.x); they run on **MUI 5** for
AmigaOS 3.x (`muimaster.library 21.x`).

**It wasn't the toolkit.** Of four showstoppers that looked like Zune↔MUI 5 incompatibilities, none
was: `Scrollgroup`/`IconList` failures and "window won't open" were all the `MUI_NewObject` −O2
miscompile; blank list rows were `RawDoFmt` `%p`; the event-broadcast crash was OS-layer
(`NP_UserData`). Portability is **layered** — OS/exec/dos (`NP_UserData`, `%p`) vs toolkit (Zune ↔ MUI
5: Scrollgroup, list/window construction); don't conflate.

### 4.1 MUI base accessor + the `MUI_NewObject` −O2 fix

- **Use the MUI 5.0 SDK** (`-DMUI_INCLUDE_DIR=$AMIGA_SDK_ROOT/MUI5/SDK/MUI/C/include`, via the
  top-level **`mui_headers`** INTERFACE target). It honours `__NOLIBBASE__`, and its
  `inline/muimaster.h` parses correctly under gcc (LPn A-variants + real `__inline` vararg
  constructors, so the `End`=`TAG_DONE)` idiom works). *Don't* use the MUI 3.8 SDK (gcc-2.x `a6@` asm)
  or regenerate it with `fd2sfd`+`sfdc` (sfdc emits the constructors as macros → breaks the `End` idiom,
  the closing `)` hidden inside `End`, invisible to the preprocessor).
- **The SDK's `__inline MUI_NewObject` is broken.** It does
  `MUI_NewObjectA(cl, (struct TagItem *)&tags)` = `&firstvararg`, which is not a valid way to reach
  the varargs and need not point at the on-stack tag array → **multi-tag objects (Window,
  Scrollgroup, Listview, custom List/Group) get garbage tags → NULL/crash** (looks like "MUI is
  broken", it's the SDK). Originally diagnosed under bebbo gcc −O2, but the construct is wrong at
  any optimization level and on any compiler — don't expect a toolchain bump to retire the fix.
  **Fix: force-include
  `include/mui_newobject_fix.h`** — a `va_list`-based `psd_MUI_NewObject` shadowed via an *object-like*
  macro. It serves both base models by binding `MUI_NewObjectA` to whatever `MUIMASTER_BASE_NAME` is
  when `<proto/muimaster.h>` is *first* included, so the binding is chosen by **include order**:
  Trident (global `MUIMasterBase`) force-includes it via `"SHELL:-include …"` (the `SHELL:` prefix stops
  CMake de-duping `-include` flags) so it's pulled first; per-instance bases (class config GUIs + popo)
  include it *after* the file-scope base is set up — one `#include "mui_newobject_fix.h"` at the end of
  `classes/mui_base.h` covers all GUI classes. *(`NO_INLINE_STDARG` is NOT a fix — it kills all stdarg
  inlines incl. `psdGetAttrs` and drops the vararg constructors so `WindowObject…End` becomes an
  undefined `MUI_NewObject` at link.)*
- **MUI base: a file-scope accessor that recovers the libbase, NOT a writable global** (ROM-safe).
  MUI's inline vararg constructors (the `…End` idiom) are compiled at include time and resolve
  `MUIMASTER_BASE_NAME` *at file scope*, so a per-call-site/function-local `#define MUIMasterBase` (the
  SAS/C trick, fine for LPn macros like poseidon/intuition) does **not** work for MUI.
  `MUIMASTER_BASE_NAME` need not be a symbol — `LP2` just loads it into `a6`, so make it a **file-scope
  expression** recovering the libbase from the current task: `SysBase->ThisTask` (what `FindTask(NULL)`
  returns, inlined — no library call per MUI op, no new global). Packaged as **`classes/mui_base.h`**
  (can't be in `common.h` — included before the class struct). At the **end of the class struct
  header**:
  ```c
  #define MUI_BASE_USERDATA struct NepClassHid   /* the struct in the GUI task's tc_UserData */
  #define MUI_BASE_FIELD    nch_MUIBase           /* its *_MUIBase field */
  #include "mui_base.h"                           /* accessor + MUIMASTER_BASE_NAME + <proto/muimaster.h> + the NewObject fix */
  ```
  Works because all MUI calls run inside the GUI subtask, spawned with the instance (or libbase) in
  `tc_UserData`. Verify per class which struct `tc_UserData` holds (bootmouse/egalaxtouch:
  `NepClassHid`/`nch_MUIBase`; massstorage: `NepClassMS`/`ncm_MUIBase`; bootkeyboard/simplemidi/
  camdmidi: the libbase `NepHidBase`/`nh_MUIBase`; eth: `NepClassEth`/`ncp_MUIBase`).
- **Link `-lamiga`** in the group — BOOPSI `DoMethod`/`DoMethodA` are amiga.lib stubs.

### 4.2 Canonical MUI 5 idioms (m68k / bebbo gcc)

Grounded in the MUI 5 SDK's `SDK/MUI/C/Examples/` (MUI-5-correct m68k gcc, SDI macros) +
MUI/Virtgroup/List/Window autodocs.

- **Custom classes / dispatcher.** Use the SDK's SDI macros (`Examples/SDI_hook.h`,
  `SDI_compiler.h`):
  ```c
  DISPATCHER(MyDispatcher) {            /* declares cl(a0) obj(a2) msg(a1), __saveds */
      switch (msg->MethodID) { case OM_NEW: return mNew(cl,obj,(APTR)msg); ... }
      return DoSuperMethodA(cl, obj, msg);
  }
  mcc = MUI_CreateCustomClass(NULL, MUIC_List, NULL, sizeof(Data), ENTRY(MyDispatcher));
  ```
  The 5th arg is a **bare function pointer** (in `a3`). **Don't use the hook's `h_Data`** — per-class
  data goes in `cl->cl_UserData`. The raw `asm()` dispatcher form Trident/popo use
  (`IPTR Xxx(struct IClass *cl asm("a0"), Object *obj asm("a2"), Msg msg asm("a1"))`) is equivalent on
  m68k (`ENTRY(f)`≡`(APTR)f`; `__saveds` is a no-op non-baserel) and is **proven** — SDI is optional
  polish. **`__saveds` is only meaningful under `-fbaserel`;** SDK examples (and we) build non-baserel,
  so it's a harmless no-op. Pick one model tree-wide; never carry `AROS_UFH3` into m68k.
- **Scrollgroup.** `MUIA_Scrollgroup_Contents` **must be a Virtgroup-class object**
  (`VirtgroupObject`/`VGroupV`/`ColGroupV(n)`), never a plain `VGroup`/`List`. It is **`I.G`** — set at
  init, *gettable but read-once*; **do not `GetAttr` it at runtime** (returns NULL on MUI 5).
  Scrollbars are created at `MUIM_Layout`, not `OM_NEW`. `MUIA_Virtgroup_TryFit, TRUE` forces a
  screen-fitting size. Keep your own pointer to the Virtgroup.
- **Lists.** MUI 5 `List` self-scrolls; `Listview` is a kept-for-compat container (safe). Prefer (a)
  builtin `MUIV_List_*Hook_StringArray` + `MUIA_List_MaxColumns`, or (b) a `MUIC_List` subclass
  overriding `MUIM_List_Construct/Destruct/Compare/Display`. Display strings must be `static`. The
  `ListviewObject + MUIA_Listview_List + <List>` form Trident uses is portable.
- **Window open.** Failure = **minimum size > screen** after MUI shrinks fonts/spacing. Mitigations:
  `MUIA_Text_SetMin, FALSE` on wide text; wrap oversized panels in
  `Scrollgroup{Contents=VirtgroupV}` (+`MUIA_Virtgroup_TryFit`); don't return huge `MinWidth/Height`
  from custom `MUIM_AskMinMax`; design to 640×200/topaz-8. **Always read back** `MUIA_Window_Open`.
- **Hooks.** Use SDI `HOOKPROTO*` + `MakeHook` (on m68k `h_Entry` = your function directly, A0=hook
  A2=obj A1=msg). Don't hand-roll `struct Hook` entries.

### 4.3 Reference facts (watch-list)

- **`AROS_LONG2BE` (~40 sites in GUI/config I/O) must stay a compile-time identity on big-endian 68k**
  (`include/aros_compat.h`) — every GUI TU must see the identity, or config IFF silently corrupts.
- **`MUIA_List_Format` ⇄ DisplayHook coupling:** the `List_Format` column count **must** match what
  the DisplayHook fills (mismatch can crash). ~17 `Listview+List` sites, all stock `MUIC_List`; most
  divergent is `CfgListClass` drag/drop (`CfgListClass.c:277‑341`, local `MUI_LPR_FULLDROP`
  redefinition). 11 DisplayHooks in 2 styles (raw `asm()` in Trident+massstorage; `AROS_UFH3` in hid),
  all hand-wired `h_Entry=`.
- **Blocking inside MUI methods is an anti-pattern** (stalls MUI's input/layout task) — present at
  `ActionClass.c:5131` (Delay), `massstorage.class.c:6140`, `hidctrl.gui.c:482`.
- **Window setup** sites to exercise: `MUIA_Window_IsSubWindow, FALSE` + live `MUIA_Window_RootObject`
  swap (`DevWinClass.c:579‑586`; `ActionClass.c:3415`); register/tab groups + `BalanceObject`
  (`ActionClass.c:3001‑3026`).

---

## 5. Debug backend (`include/debug.h`)

Mirrors emu68-driver-stack's scheme so logs surface on the **Pi console** during PiStorm/Emu68
debugging. One shared header-only formatter; every existing `KPRINTF(level,(fmt,…))` / `XPRINTF` / `DB`
call site is **unchanged**. Selected at configure time via `-DPOSEIDON_DEBUG_BACKEND=`:

| Backend | Compile defs | `putch` sink | Links debug.lib? |
|---|---|---|---|
| `pistorm` (default) | `DEBUG DB_LEVEL=<n>` | `*(volatile UBYTE*)0xdeadbeef = c` (Emu68 traps → Pi console) | no |
| `serial` | `DEBUG DEBUG_SERIAL DB_LEVEL=<n>` | `KPutChar(c)` (debug.lib) | yes (per target, via `psd_debug_finalize()`) |
| `off` | *(none)* | formatter not emitted | no |

- **`include/debug.h`** is the canonical header (the 4 per-component `debug.{h,c}` were deleted).
  `psd_kprintf` → classic exec **`RawDoFmt`** (hence the `%p` → `0x%08lx` rule, §1.5) → `psd_putch`
  (`asm("d0")`/`asm("a3")` callback, the ABI already proven by `psdSafeRawDoFmtA`/`pPutChar`).
- **Levels:** `KPRINTF(l,x)` emits iff `l >= DB_LEVEL`. `-DPOSEIDON_DEBUG_LEVEL=<n>` (default 1 = all;
  higher = quieter). `XPRINTF` ≡ `KPRINTF`.
- **`PSF_KLOG` removed** — the old runtime "mirror error-log to KPrintF" boot-arg path is gone, so the
  framework is purely compile-time and `off`/`pistorm` link no debug.lib. The error log itself
  (`PsdErrorlog`) is untouched.
- **cmake:** `cmake/PoseidonDebug.cmake` → `psd_debug_definitions()` (top-level, global defs) +
  `psd_debug_finalize(<target>)` (per target — links `-ldebug` + a weak `__divsi3` glue **only** for
  `serial`). See also the deploy flow (`build.sh`, AE.exe → Amiga).
