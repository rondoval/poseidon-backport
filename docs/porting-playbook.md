# Porting playbook (de-AROS → AmigaOS 3.2)

How to port a Poseidon component (genmodule / AROS) into a clean NDK 3.2 / bebbo-gcc / cmake one.
The C *logic* ports almost untouched; the work is replacing the genmodule calling-convention glue
(§1) and supplying the AROS vocabulary (§1.5). The mechanical parts are automated by two scripts in
`scripts/`: **`conf2sfd.py`** and **`dearos_lh.py`**.

The port is complete; this is a recipe, not a record — use it to port an AROS fix into a component,
add a class driver, or touch the `.sfd`/`sfdc` flow.

---

## 0. Scope — what was taken and what was dropped

From the AROS `rom/usb/` tree (baseline SHA and extraction tag: CLAUDE.md): `poseidon.library`,
`usbclass.library` (the base meta-class every class inherits — a headers-only cmake target, since
nothing opens it), all class drivers, Trident and the CLI tools.

**Dropped:** the AROS host controllers `pciusb`/`pcixhci`/`vusbhc` — this port drives a host
controller through a `.device`, and the lower-edge contract is
[poseidon-context-hcd-abi.md](poseidon-context-hcd-abi.md); `felsunxi` (Allwinner FEL) and
`guiapps/ps3eye`, irrelevant on metal; the `trident/catalogs` *submodule link* (every translation was
copied into the tree).

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
- **The LVO function name is parenthesised** `(psdAllocVec)(…)` so the same-named *inline call macro*
  (used for the library's own internal calls) does not expand at the definition.
- `AROS_UFH`/`AROS_UFP` (hook callbacks / their prototypes) → same, but **no `a6`**.
- Also drops `AROS_LIBFUNC_INIT/EXIT` and `ADD2{INIT,OPEN,EXPUNGE}LIB`, and rewrites
  `GM_UNIQUENAME(x)`→`x`, `LIBBASETYPEPTR`→`struct <Base> *`.
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
  writable static breaks ROM-ability.

### 1.4 Base-variable & `__NOLIBBASE__`

Compile every component `-D__NOLIBBASE__` so no proto auto-declares a base global; the component
supplies each base itself:
- **Its own API** (internal calls): `#define <M>_BASE_NAME ps` before `<inline/<m>.h>` — `ps` is the
  libbase parameter present in every LVO function. So `psdAllocVec(size)` → LVO via `a6 = ps`.
- **System libs** it opens: `#define DOSBase ps->ps_DosBase` etc.; `SysBase` is the real global.
- **Other Poseidon libs** it calls (usbclass): the existing `#define UsbClsBase puc->puc_ClassBase`
  satisfies `USBCLASS_BASE_NAME`.
- **Clients** (classes/tools) use their *own* base var (`ps` + `__<M>_NOLIBBASE__`, or
  `#define <M>_BASE_NAME <theirvar>`); the `.sfd` `==base` is `_<M>Base`, so reconcile the client's
  base-var name.

### 1.5 `aros_compat.h` — the AROS-vocabulary shim

`include/aros_compat.h`, **force-included** (`-include`) into every TU, supplies what the NDK lacks
(so the de-AROS'd sources need no per-file type edits). Read the header for the current set; the
entries that are not self-evident are `_sfdc_vararg` = `APTR` (so vararg string-literals are `void*`
rather than a wide-char error), a self-contained inline `stricmp` (libc `strcasecmp` drags in
`malloc.o` → unresolved `SysBase` in the freestanding link), the byte-order macros (`*2BE` identity
on m68k, `*2LE`/`LE2*` = `__builtin_bswap` since USB is LE — every GUI TU must see the identity or
config IFF silently corrupts), and `AROS_SLOWSTACKFORMAT_{PRE,ARG,POST}` (the m68k stack-varargs
pointer idiom, `ARG = &x+1`). Extend it (guarded with `#ifndef`) whenever a new component trips over
another AROS-ism.

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

Flags live at exactly one of three levels — toolchain (`cmake/toolchain.cmake`), tree-wide (root
`add_compile_options()`), per target — and are **never repeated per target**. CLAUDE.md lists what
each level sets; every blanket flag carries its rationale in the file that sets it, and the
freestanding link line is defined once, in the root `CMakeLists.txt`, and inherited.

A new component adds only its `-O` level, `-ffreestanding` if it is a library or class (a **COMPILE**
option — as a link-only flag it is silently inert), `-D__NOLIBBASE__`, its include dirs (`include/`,
the component, the generated sfd dir), `psd_debug_finalize(<target>)` (§6), and `OUTPUT_NAME <m>` +
`SUFFIX ".library"`; reuse `cmake/GenerateSfdHeaders.cmake`. Two traps: any *second* force-include
beyond `aros_compat.h` (Trident's `mui_compat.h`) must use the `"SHELL:-include …"` form, since CMake
de-dups a bare repeated `-include` flag; and the release builds the same tree for 68020/68040/68060,
so never assume a CPU in code.

---

## 2. Component-specific AROS-isms

| AROS-ism | Where | Replacement |
|---|---|---|
| MUI GUI | boot-class config dialogs, `popo.gui.c`, Trident | real MUI via **MUI 5.0** + a file-scope MUI-base accessor (no global) + `-lamiga` — see §4. |
| `debug.h` / `KPRINTF` / `XPRINTF` / `DB` | every component | the shared **`include/debug.h`** switchable backend (§6); call sites unchanged. AROS `bug()`/`D()` map to `KPRINTF` when ported. |
| OOP / HIDD (`<oop/oop.h>`, `<hidd/*>`) | **`hid` class only**, behind `#if __AROS__` | the blocks have no native `#else`, so bebbo **auto-excludes** them; use the original `input.device` path (compare `bootmouse.class.c`). |
| `(HOOKFUNC)func` cast on `h_Entry` | hook assignments (shellapps, Trident) | gcc errors `-Wincompatible-pointer-types` via the *typedef* even though `HOOKFUNC`≡`ULONG(*)()`; cast to the literal `(ULONG (*)(void))` (or `(APTR)`) instead. |
| NDK inlines typed `RET (*)()` (`SetFunction`, `RawDoFmt`, `Interrupt.is_Code`) | patches, formatters, interrupt servers | GCC 15+ defaults to **C23**, where `()` means `(void)` — a prototyped function (ours all carry `asm("dN")` register args) no longer converts implicitly and it is a hard **error**, not a warning. Cast explicitly, spelled `(ULONG (*)(void))` / `(void (*)(void))`: correct in both C11 and C23. An `APTR`-typed variable still passes silently (GCC's `void*`↔function-pointer extension) — that is why only *some* call sites break. |
| `ADD2INIT/EXIT` linker sets | Trident `locale.c` | explicit init/cleanup in `main()`. |
| runtime debug knobs (`bootloader.resource` `usbdebug`, `PSF_KLOG`) | `libInit`, error-log path | dropped — the framework is compile-time (§6). |
| unchecked `psdAddHardware()` / `psdRemHardware()` | `poseidon.library` hardware list | **deliberate behavioural divergence**, not a port artefact: both validate, where AROS leaves it to the caller — a duplicate device+unit is refused, a `phw` that is not on the list is ignored rather than followed, each logged at `RETURN_WARN`, matching the guard `psdAddClass()` has always had. Identity lives in **`include/hwmatch.h`**, which `pFindHardware()` compares through too, so library and clients cannot drift. Don't re-implement the walk in a new caller. |

---

## 3. Class drivers

Each class is a full genmodule library, so §1 runs **per class**, but the shared skeleton + GUI/ROM
infrastructure make most of it mechanical. No per-class `.sfd` (the 3 usbclass ABI vectors come from
`usbclass_headers` + the shared `class_main.c` skeleton). The installer drops any new `.class` into
`SYS:Classes/USB/`, so there is **no packaging work per class**.

### 3.1 Per-class recipe (mechanical core)

1. **`dearos_lh.py --inplace`** the `<name>.class.c` *and* its `.h`; **`%p` sweep** (§1.5).
2. **CMakeLists** via `add_poseidon_class()` (§3.2): set `CLASS_NAME`/`VERSION`/`REVISION`/`PRI`,
   `HAS_LIBOPEN`/`HAS_LIBCLOSE` as the source needs; link `poseidon_headers` (+ `mui_headers` if GUI,
   + `DEPS` for family headers).
3. **GUI classes:** the class struct header ends with the `mui_base.h` block (`MUI_BASE_USERDATA` +
   `MUI_BASE_FIELD` + `#include "mui_base.h"`) — that include also pulls the `MUI_NewObject` fix (§4).
4. **Start ROM-clean** — the `$4` exec base and `const` tables come from `class_main.c`/`common.h`;
   don't add a writable global (§3.3; the rule and its silent failure mode are in
   [rom-image.md](rom-image.md)).
5. **New `aros_compat.h` vocab / non-NDK headers** carried into `include/` as they surface.
6. **Build 0/0**; deploy adds it to the install automatically.

### 3.2 `add_poseidon_class()` CMake helper

Defined and documented at the top of the root `CMakeLists.txt` (next to `generate_sfd_headers`); it
collapses a class's ~40-line CMakeLists to ~3 lines. **Read the signature there, not here** —
`GUI` adds `mui_headers` + `-lamiga`, `AMIGALIB` adds `-lamiga` alone, `DEPS` carries family header
targets, `INCLUDE` overrides the default `<name>.h` struct header. The class version is global, so
there is nothing to state per class.

Header availability for a new class: the **AHI** sub-driver headers (`libraries/ahi_sub.h`,
`defines/`, `inline/`, `proto/`, `devices/ahi.h`), serial, `input.device` and `usbparallel` are
already in the bebbo toolchain / NDK. **SANA-II** is not in bebbo's default include — it comes from
the `sana2_headers` INTERFACE target (`SANA2_INCLUDE_DIR`, defaulted under `AMIGA_SDK_ROOT` so no
literal home path lands in the build), as does the MUI 5 SDK via `mui_headers` (§4). **CAMD** is
absent; camdmidi vendors only `include/midi/camddevices.h`.

### 3.3 Class skeleton & embedded-device recipe

The shared `class_main.c` + `common.h` bake in the **`$4` exec base** and `const` tables. Romtag name
need not equal dir name — pass the *romtag* name to `add_poseidon_class` so `<name>.class.c`/`.h`
resolve; the dir is only the `add_subdirectory` path.

**Embedded-device (dual-library) recipe** (massstorage / serial / eth / audio / arosx): the class
`MakeLibrary`'s a second library/device in `libInit`.
- dearos with **two** `--libbasetype`s: the class base for `<name>.class.c`/`.h`, the device base for
  `dev.c` (its `a6` is the *device* base, not the class base). When both bases appear in one file,
  disambiguate by basevar and re-type only the funcs whose `a6` carries the second base. **Watch for
  a second basetype macro that is a *substring* of `LIBBASETYPEPTR`** (audio's `SUBLIBBASETYPEPTR`):
  resolve it and drop its `#define` *before* the transform, or a blind replace corrupts it.
- `dev.h` uses `AROS_LD`/`AROS_LDA` (library-descriptor protos) which `dearos_lh.py` does **not**
  handle → hand-convert to plain `asm()` protos (template `massstorage/dev.h`). The one `AROS_LC1`
  self-call → a direct call; `&AROS_SLIB_ENTRY(devX, dev, n)` in `DevFuncTable[]` → `(APTR) devX`.
  `AROS_INTH1` (soft-int handlers) → `ULONG name(type var asm("a1"))` — `is_Data` arrives in A1.
- un-`static` `libInit`/`libExpunge` (skeleton calls them via `extern`); add `BPTR nh_SegList;` to the
  class base; replace any global `SysBase` value-use → `EXEC_BASE_NAME`.
- **`bug()`** (AROS raw debug) → `KPRINTF(10, (…))`; fix `%d`→`%ld` (classic `RawDoFmt` `%d` reads 16
  bits; args are 32-bit).
- A zero-init `const` sentinel lands in `.bss`; an explicit `= {0}` initializer keeps it in `.rodata`
  so `.bss`=0.

**Class extra vectors** (generic, gated): `class_main.c` supports `-DCLASS_VECTORS_HDR="…"` to pull a
class header that declares externs and `#define CLASS_EXTRA_VECTORS` (the funcTable tail: reserved
`LibNull`s + the extra vectors). Absent for every other class ⇒ their binaries stay byte-identical.
Used by camdmidi, whose `CMakeLists.txt` documents the whole embedded-CAMD-driver pipeline.

**Definition of done (per class):** builds 0/0; ROM-clean (`.bss`=0, no new named writable globals —
`nm` check); `%p`-free debug strings; GUI (if any) opens via Trident's Classes panel; deployed
`.class` binds its device on plug.

---

## 4. MUI GUIs (build against MUI 5, run on MUI 3.8+)

Trident and the class config GUIs came from AROS Zune (~MUI 3.x). They **compile** against the MUI 5
SDK (§4.1 — a toolchain choice only) and **run** on `muimaster.library` **19+**, MUI 3.8/4.0/5 alike
(§4.2 — a runtime floor). The two are independent.

**Suspect the OS layer before the toolkit.** Of four showstoppers that looked like Zune↔MUI 5
incompatibilities, none was: `Scrollgroup`/`IconList` failure and "window won't open" were the
`MUI_NewObject` bug (§4.1), blank list rows were `RawDoFmt` `%p`, the event-broadcast crash was
`NP_UserData` (both §1.5).

### 4.1 SDK, the `MUI_NewObject` fix, and the base accessor

**Use the MUI 5.0 SDK** (`-DMUI_INCLUDE_DIR=…/MUI5/SDK/MUI/C/include`, via the `mui_headers`
INTERFACE target): it honours `__NOLIBBASE__` and its `inline/muimaster.h` parses under gcc (LPn
A-variants + real `__inline` vararg constructors, so `End`=`TAG_DONE)` works). Not the MUI 3.8 SDK
(gcc-2.x `a6@` asm), and don't regenerate it with `fd2sfd`+`sfdc` — sfdc emits the constructors as
function-like macros, which kills the `End` idiom.

**The SDK's `__inline MUI_NewObject` is broken and always will be**, so never use it: it passes
`&tags` — the address of the first *named* vararg — as the tag array, leaving the `...` args dead for
the inliner to drop. On **gcc 16.1** a 5-attribute `WindowObject` (11 tag words + terminator) becomes

```
subq.l #4,sp                 ; ONE longword reserved
move.l #-2143113923,4(sp)    ; only MUIA_Window_Title stored
lea (4,sp),a1                ; a1 = &tags, passed to MUI_NewObjectA
```

— no values, no `TAG_DONE`; MUI walks off the end. Surviving tag words by tier: `-O0` 9, then **1**
at `-O1`/`-O2`/`-O3`/`-Os` (10 with the fix), so every tier we ship is affected and `-O0` passing is
why it looks like an optimizer bug. `MUI_MakeObject` escapes it — `Label(x)` and friends are always
fully parenthesised. The replacement, and why include order picks the MUI base, is documented in the
force-included `include/mui_compat.h`.

**The MUI base is a file-scope accessor, never a writable global** (ROM-safe); `classes/mui_base.h`
carries it and the reasoning. A GUI class ends its struct header with:

```c
#define MUI_BASE_USERDATA struct NepClassHid   /* the struct in the GUI task's tc_UserData */
#define MUI_BASE_FIELD    nch_MUIBase          /* its *_MUIBase field */
#include "mui_base.h"                          /* accessor + base name + proto + the NewObject fix */
```

and links **`-lamiga`** (BOOPSI `DoMethod`/`DoMethodA` are amiga.lib stubs). Verify per class which
of the instance or the libbase its GUI subtask carries in `tc_UserData` — the existing classes show
both, each stated in its own `CMakeLists.txt` header. Dispatchers are either the raw `asm()` form
Trident and popo use (`cl` in `a0`, `obj` in `a2`, `msg` in `a1`) or the SDK's SDI `DISPATCHER()`,
equivalent on m68k; per-class data goes in `cl->cl_UserData`, not the hook's `h_Data`. **Never carry
`AROS_UFH3` into m68k.**

### 4.2 The MUI 3.8 floor

The fleet runs on `muimaster.library` **19+**, and that was nearly free: Zune-derived code never grew
MUI 4/5 dependencies. `include/mui_compat.h` shadows the SDK's `MUIMASTER_VMIN` (20, the MUI 4
baseline) with 19, so every `OpenLibrary(MUIMASTER_NAME, MUIMASTER_VMIN)` picks the floor up
untouched and a class copied from an existing one cannot quietly regress it. The ceiling is **V14**
(`MUIM_Application_AboutMUI`, `Trident.c`); keep it there.

`scripts/check-mui38.py` runs at the end of every container build and hard-fails on a symbol 3.8
lacks, anything V≥20, or an `OpenLibrary` bypassing the shadow; there is no escape hatch by design.
Its docstring is the full account, including how to regenerate the `scripts/mui38-symbols.tsv`
oracle. The one thing it merely *warns* about is access-flag narrowing: the `isg` flags narrow for 29
attributes between MUI 5 and 3.8, and exactly one is used in the lost direction
(`MUIA_Cycle_Entries`, `ActionClass.c`), where a comment rather than a workaround marks it.

---

## 5. The OS 3.1 floor

Same shape as §4.2, one layer down: we **compile** against the NDK 3.2 headers but **run** on
Kickstart/Workbench **3.1 (V40)** and up. The asymmetry is the whole hazard — a V47 call compiles
without a murmur and fails only on the user's machine, and it fails *silently* when it is a device
command rather than a library open.

**Re-run these two sweeps when porting an AROS fix or adding a class driver**; both must stay empty.
There is deliberately no `check-os31.py` — they are cheap by hand, and that is when to run them.

1. **Functions.** The NDK 3.2 `SFD/*.sfd` files carry `==version N` markers giving the library
   version each function was introduced in. Take every name under a marker above 40 — `IconControl`,
   `GetIconTags`, `WorkbenchControl`, `OpenWorkbenchObject`, `NewMinList`, `SNPrintf`, `Strncpy`,
   `IntuitionControl`, `ShowWindow`/`HideWindow`, `LayerOccluded`, `ScaleGadgetRect`, the V47
   outline-font calls — and grep for it. **Zero hits.**
2. **Constants and tags.** Every `#define` in `NDK3.2R4/Include_H` whose line carries a `V41`…`V59`
   annotation — 42 of them — grepped the same way. **One hit**: `IND_ADDEVENT`.

Plus the direct check: no `OpenLibrary("<os library>", N)` literal above 40. There is no carve-out
below it either — the vendored `mounter/` submodule is V40-only like the rest of the tree; its
`dos.library` open is still allowed to *fail*, but that is a pre-DOS detector, not a version
fallback.

Three exceptions stand, each runtime-gated rather than avoided, and each documented where it lives:
`IND_ADDEVENT` (`input.device` V47) in `hid.class`, gated on the device's own `lib_Version >= 47`
with an `IND_WRITEEVENT` fallback ([hid.class-architecture.md](hid.class-architecture.md) §7.1);
`WBAPPMENUA_GetTitleKey`/`WBAPPMENUA_UseKey` (`workbench.library` V45) in USBEject, *probed* rather
than version-tested, so an older library simply ignores the tag (`usbeject/USBEject.c`); and
`SetJoyPortAttrsA`, which is a *revision* — `lowlevel.library` **V40.27** — that `OpenLibrary()`
cannot ask for, so `nInstallLLPatch()` measures `lib_NegSize` before `SetFunction()`ing LVO −132
(`hid.class-architecture.md` §8). Prefer that last shape — ask the object what it has — wherever a revision, not a
version, is what actually differs. Anything newer than V40 needs a gate written on purpose, and a
line here saying which and where.

---

## 6. Debug backend

`include/debug.h` (the shared header-only formatter) and `cmake/PoseidonDebug.cmake` (the `-DPOSEIDON_DEBUG_BACKEND=pistorm|serial|off` and
`-DPOSEIDON_DEBUG_LEVEL=<n>` knobs) document themselves. What matters when porting:

- Every `KPRINTF(level,(fmt,…))` / `XPRINTF` / `DB` call site is **unchanged** by the port; AROS
  `bug()`/`D()` become `KPRINTF` (§3.3). Output goes through classic exec `RawDoFmt`, hence the
  `%p`/`%ld` rules of §1.5.
- **A new target must call `psd_debug_finalize(<target>)`** — it links `-ldebug` plus a weak
  `__divsi3` glue, and only for the `serial` backend.
- The framework is purely compile-time; there is no runtime verbosity knob. The error log itself
  (`PsdErrorlog`) is untouched by any of it.
