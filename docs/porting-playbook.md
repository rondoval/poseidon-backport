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

**Kept but unported:** `poseidon.library/usbrom{early,late}startup.c` (ROM-resident autostart) — in
the tree, not compiled, still carrying AROS-isms and references to the dropped HW drivers. A resident
stack loader supersedes them (ROM-ability phase in [implementation-plan.md](implementation-plan.md)).

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
pointer idiom, `ARG = &x+1`).

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

Flags live at exactly one of three levels — toolchain (`cmake/toolchain.cmake`), tree-wide (root
`add_compile_options()`), per target — and are **never repeated per target**; CLAUDE.md lists what
each level sets. A new component adds only its `-O` level, `-ffreestanding` if it is a library or
class (a **COMPILE** option — as a link-only flag it is silently inert), `-D__NOLIBBASE__`, and its
include dirs (`include/`, the component, the generated sfd dir). The release builds the same tree for
68020/68040/68060, so never assume a CPU in code.

- `-Wno-array-bounds` (toolchain) silences GCC ≥ 12's false positive on the absolute-`$4`
  `EXEC_BASE_NAME` idiom — it fires at every call site, so a pragma cannot scope it.
- `-Wno-int-conversion` (tree-wide) is **approved and load-bearing**, not debt: m68k tag/vararg calls
  inherently mix pointers and ULONGs through sfdc's vararg array (pointers are ULONG-sized here; gcc
  ignores `__attribute__((iptr))`), and GCC 14+ makes int↔pointer conversion an *error*.
- Any *second* force-include beyond `aros_compat.h` (Trident's `mui_compat.h`) must use the
  `"SHELL:-include …"` form — CMake de-dups a bare repeated `-include` flag.
- **Link freestanding:** `-nostdlib -nostartfiles -s -Wl,-e,_doNotExecute`, libs
  `-Wl,--start-group -lc -lgcc -Wl,--end-group` (`-lc` libnix string fns; `-lgcc` intrinsics —
  grouped so `__divsi3` resolves). **`-ldebug` is not hardcoded** — added per target by
  `psd_debug_finalize()` **only** for the `serial` debug backend (§5).
- `OUTPUT_NAME <m>` + `SUFFIX ".library"`. Reuse `cmake/GenerateSfdHeaders.cmake`.

---

## 2. Component-specific AROS-isms

| AROS-ism | Where | Replacement |
|---|---|---|
| MUI GUI | boot-class config dialogs, `popo.gui.c`, Trident | real MUI via **MUI 5.0** + a file-scope MUI-base accessor (no global) + `-lamiga` — see §4. |
| `debug.h` / `KPRINTF` / `XPRINTF` / `DB` | every component | the shared **`include/debug.h`** switchable backend (§5); call sites unchanged. AROS `bug()`/`D()` map to `KPRINTF` when ported. |
| OOP / HIDD (`<oop/oop.h>`, `<hidd/*>`) | **`hid` class only**, behind `#if __AROS__` | the blocks have no native `#else`, so bebbo **auto-excludes** them; use the original `input.device` path (compare `bootmouse.class.c`). |
| `(HOOKFUNC)func` cast on `h_Entry` | hook assignments (shellapps, Trident) | gcc errors `-Wincompatible-pointer-types` via the *typedef* even though `HOOKFUNC`≡`ULONG(*)()`; cast to the literal `(ULONG (*)(void))` (or `(APTR)`) instead. |
| NDK inlines typed `RET (*)()` (`SetFunction`, `RawDoFmt`, `Interrupt.is_Code`) | patches, formatters, interrupt servers | GCC 15+ defaults to **C23**, where `()` means `(void)` — a prototyped function (ours all carry `asm("dN")` register args) no longer converts implicitly and it is a hard **error**, not a warning. Cast explicitly, spelled `(ULONG (*)(void))` / `(void (*)(void))`: correct in both C11 and C23. An `APTR`-typed variable still passes silently (GCC's `void*`↔function-pointer extension) — that is why only *some* call sites break. |
| `ADD2INIT/EXIT` linker sets | Trident `locale.c` | explicit init/cleanup in `main()`. |
| runtime debug knobs (`bootloader.resource` `usbdebug`, `PSF_KLOG`) | `libInit`, error-log path | dropped — the framework is compile-time (§5). |

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
   don't add a writable global (§3.3, and the standard in
   [implementation-plan.md](implementation-plan.md)).
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
(§4.3 — a runtime floor). The two are independent.

**Suspect the OS layer before the toolkit.** Of four showstoppers that looked like Zune↔MUI 5
incompatibilities, none was: `Scrollgroup`/`IconList` failure and "window won't open" were the
`MUI_NewObject` bug (§4.1), blank list rows were `RawDoFmt` `%p`, the event-broadcast crash was
`NP_UserData` (both §1.5).

### 4.1 SDK, the `MUI_NewObject` fix, and the base accessor

**Use the MUI 5.0 SDK** (`-DMUI_INCLUDE_DIR=…/MUI5/SDK/MUI/C/include`, via the `mui_headers`
INTERFACE target): it honours `__NOLIBBASE__` and its `inline/muimaster.h` parses under gcc (LPn
A-variants + real `__inline` vararg constructors, so `End`=`TAG_DONE)` works). Not the MUI 3.8 SDK
(gcc-2.x `a6@` asm), and don't regenerate with `fd2sfd`+`sfdc` — sfdc emits the constructors as
function-like macros, which kills the `End` idiom (see below).

**The SDK's `__inline MUI_NewObject` is broken and always will be.** It does
`MUI_NewObjectA(cl, (struct TagItem *)&tags)` — the address of the first *named* vararg, assuming the
rest follow contiguously. The `...` args are never read via `va_arg`, so the inliner drops them as
dead. Not a miscompile: invalid C, which newer compilers punish harder. Re-verified on **gcc 16.1** —
a 5-attribute `WindowObject` (11 tag words + terminator) becomes

```
subq.l #4,sp                 ; ONE longword reserved
move.l #-2143113923,4(sp)    ; only MUIA_Window_Title stored
lea (4,sp),a1                ; a1 = &tags, passed to MUI_NewObjectA
```

— no values, no `TAG_DONE`; MUI walks off the end. Surviving tag words by tier: `-O0` 9, then **1**
at `-O1`/`-O2`/`-O3`/`-Os` (10 with the fix). Every tier we ship is affected; `-O0` passing is why it
looks like an optimizer bug.

**Fix: force-include `include/mui_compat.h`** — a `va_list`-based `psd_MUI_NewObject` shadowed by an
**object-like** macro, so `XxxObject … End` still expands to a plain call. Object-like is forced, not
preferred: a function-like macro needs its closing `)`, which lives inside `End`'s expansion,
invisible while arguments are collected → *"unterminated argument list invoking macro"* (the same
reason sfdc's constructors fail). The SDK's `MUI_MakeObject` escapes the bug because `Label(x)` and
friends are always fully parenthesised, so they need no fix. The `va_list`→`struct TagItem *` cast is
sound only because m68k gcc's `va_list` is a plain pointer. *(`NO_INLINE_STDARG` is NOT an
alternative: it kills all stdarg inlines incl. `psdGetAttrs` and drops the vararg constructors, so
`WindowObject…End` becomes an undefined symbol at link.)* The fix binds `MUI_NewObjectA` to whatever
`MUIMASTER_BASE_NAME` is when `<proto/muimaster.h>` is *first* pulled, so **include order picks the
base**: Trident (global `MUIMasterBase`) force-includes it (§1.6); per-instance bases get it from the
end of `classes/mui_base.h`.

**MUI base: a file-scope accessor, never a writable global** (ROM-safe). MUI's inline constructors
resolve `MUIMASTER_BASE_NAME` at *file* scope, so a function-local `#define MUIMasterBase` (the SAS/C
trick, fine for LPn macros like poseidon/intuition) does **not** work. It need not be a symbol —
`LP2` only loads it into `a6` — so make it a file-scope *expression* recovering the libbase from the
running task via `SysBase->ThisTask` (inlined; no library call per MUI op, no global). Packaged as
`classes/mui_base.h` (not `common.h`, which precedes the class struct); at the end of the class
struct header:

```c
#define MUI_BASE_USERDATA struct NepClassHid   /* the struct in the GUI task's tc_UserData */
#define MUI_BASE_FIELD    nch_MUIBase          /* its *_MUIBase field */
#include "mui_base.h"                          /* accessor + base name + proto + the NewObject fix */
```

Works because every MUI call runs in the GUI subtask, spawned with the instance (or libbase) in
`tc_UserData` — verify per class which of the two its subtask carries; the existing classes show
both, each stated in its own `CMakeLists.txt` header. **Link `-lamiga`** — BOOPSI
`DoMethod`/`DoMethodA` are amiga.lib stubs.

### 4.2 Canonical MUI idioms (m68k / bebbo gcc)

From the MUI 5 SDK's `Examples/` + the MUI/Virtgroup/List/Window autodocs.

- **Custom classes.** `MUI_CreateCustomClass(NULL, MUIC_List, NULL, sizeof(Data), dispatcher)` — 5th
  arg is a **bare function pointer** (in `a3`); per-class data goes in `cl->cl_UserData`, **not** the
  hook's `h_Data`. Two equivalent dispatcher forms: the SDK's SDI `DISPATCHER()`/`ENTRY()`, or the raw
  `asm()` form Trident/popo use (`IPTR f(struct IClass *cl asm("a0"), Object *obj asm("a2"), Msg msg
  asm("a1"))`). On m68k `ENTRY(f)`≡`(APTR)f` and `__saveds` is a no-op non-baserel, so the raw form is
  equivalent and **proven**; SDI is polish. Pick one tree-wide; never carry `AROS_UFH3` into m68k.
- **Scrollgroup.** `MUIA_Scrollgroup_Contents` **must** be a Virtgroup-class object
  (`VirtgroupObject`/`VGroupV`/`ColGroupV(n)`), never a plain `VGroup`/`List`. It is `i.g` in **both**
  SDKs — set at init; **don't `GetAttr` it at runtime** (NULL on MUI 5 — a behaviour note, not an ABI
  fact, so don't "fix" it against 3.8). Keep your own pointer. Scrollbars appear at `MUIM_Layout`,
  not `OM_NEW`.
- **Lists.** MUI 5 `List` self-scrolls; `Listview` is a compat container (safe). Prefer builtin
  `MUIV_List_*Hook_StringArray` + `MUIA_List_MaxColumns`, or a `MUIC_List` subclass overriding
  `MUIM_List_Construct/Destruct/Compare/Display`. Display strings must be `static`, and
  **`MUIA_List_Format`'s column count must match what the DisplayHook fills** — a mismatch can
  crash. Trident's `ListviewObject + MUIA_Listview_List + <List>` form is portable.
- **Window open.** Failure = **minimum size > screen** after MUI shrinks fonts/spacing. Design to
  640×200/topaz-8; `MUIA_Text_SetMin, FALSE` on wide text; wrap oversized panels in
  `Scrollgroup{Contents=VirtgroupV}`; don't return huge `MinWidth/Height` from a custom
  `MUIM_AskMinMax`. **Always read back `MUIA_Window_Open`.** (`MUIA_Virtgroup_TryFit` also fits a
  group to the screen, but it is V20 — MUI 5 only, §4.3.)
- **Hooks.** SDI `HOOKPROTO*` + `MakeHook`; on m68k `h_Entry` = your function directly (A0=hook,
  A2=obj, A1=msg). Don't hand-roll `struct Hook` entries.

### 4.3 The MUI 3.8 floor

The fleet runs on `muimaster.library` **19+**, and that was nearly free: Zune-derived code never grew
MUI 4/5 dependencies. Resolving every MUI identifier in the tree — *including transitive expansion of
the `XxxObject`/`Label`/`RegisterGroup` macros, without which a token scan is blind* — against the
MUI 5 SDK's `/* V20 isg */` annotations and the 3.8 header found no symbol, LVO or tag value the
fleet needs and 3.8 lacks. The ceiling is **V14** (`MUIM_Application_AboutMUI`, `Trident.c`); keep it
there.

**The floor is a shadow, not 25 edits.** `include/mui_compat.h` `#undef`s the SDK's `MUIMASTER_VMIN`
(20 = the MUI 4 baseline) and redefines it to 19, so every `OpenLibrary(MUIMASTER_NAME,
MUIMASTER_VMIN)` picks it up untouched — and a class copied from an existing one cannot regress it.
It sits in the same force-included header as the `MUI_NewObject` fix (§4.1), which is the one header
every MUI TU reaches under both base models.

Symbol presence is not the whole story: the `isg` access flags narrow for 29 attributes between MUI 5
and 3.8. Exactly one is used in the lost direction (`MUIA_Cycle_Entries`, `ActionClass.c`), and it
carries a comment rather than a workaround — see `implementation-plan.md` §5.5.

**Enforcement.** `scripts/check-mui38.py` runs at the end of every container build and hard-fails on
a symbol 3.8 lacks, anything V≥20, or an `OpenLibrary` bypassing the shadow; it warns on access-flag
narrowing (`--strict` promotes). Comments and string literals are stripped first, so naming an
attribute in prose is not a dependency. There is no escape hatch by design — needing something newer
means adding runtime gating deliberately. Oracle: `scripts/mui38-symbols.tsv`, since the 3.8 archive
is not in the container; regenerate with
`scripts/check-mui38.py --gen-inventory <mui38-SDK-root> > scripts/mui38-symbols.tsv`.

---

## 5. Debug backend (`include/debug.h`)

One shared header-only formatter, mirroring emu68-driver-stack's scheme so logs surface on the **Pi
console** under PiStorm/Emu68. Every `KPRINTF(level,(fmt,…))` / `XPRINTF` / `DB` call site is
**unchanged** by the port; AROS `bug()`/`D()` become `KPRINTF` (§3.3).

- Backend selection (`-DPOSEIDON_DEBUG_BACKEND=pistorm|serial|off`) and verbosity
  (`-DPOSEIDON_DEBUG_LEVEL=<n>`; `KPRINTF(l,x)` emits iff `l >= DB_LEVEL`) are documented in
  `cmake/PoseidonDebug.cmake` and `include/debug.h` — the canonical header, which replaced the 4
  per-component `debug.{h,c}`.
- Output goes through classic exec **`RawDoFmt`** → `psd_putch` (`asm("d0")`/`asm("a3")` callback,
  the ABI already proven by `psdSafeRawDoFmtA`/`pPutChar`) — hence the `%p`/`%ld` rules of §1.5.
- **A new target must call `psd_debug_finalize(<target>)`** — it links `-ldebug` plus a weak
  `__divsi3` glue, and only for the `serial` backend.
- The runtime `PSF_KLOG` "mirror error-log to KPrintF" boot-arg is gone; the framework is purely
  compile-time. The error log itself (`PsdErrorlog`) is untouched.
