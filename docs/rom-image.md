# The Kickstart ROM image

**The instructions users follow are not here.** They are
[`dist/ROM-ReadMe.md`](../dist/ROM-ReadMe.md), shipped in the archive's `ROM/` drawer under that
same name — requirements, one command, the `config.txt` edit, rollback and troubleshooting, with
none of the below. Keep the two in step: anything that changes what a user types belongs in both.

## What it buys

* A USB **mouse and keyboard in the early boot shell and the boot menu** — before
  `startup-sequence`, and on a machine with no PS/2-style Amiga keyboard attached at all.
* **Booting from a USB drive**, and from NVMe if you embed `nvme.device`.

Without it, `C:PsdStackLoader` starts the stack from `startup-sequence`, which is far too late for
either.

## How it works

Emu68 accepts a 2 MB Kickstart and maps it in four 512 KiB chunks: chunk 0 at `$E00000`, chunk 1 at
`$A80000`, chunk 2 at `$B00000` — so chunks 1 and 2 form one contiguous megabyte — and chunk 3 at
`$F80000`, which is the Kickstart proper. The image the script builds is:

```
[ 512 KiB Kickstart mirror @ $E00000 ][ 1 MB extension @ $A80000 ][ 512 KiB patched Kickstart ]
```

Chunk 0 is not spare space. While OVL is set — which it is at reset — Emu68 answers the whole first
512 KiB of the address space out of its `$E00000` shadow, so the reset vector itself (SSP and PC,
the first two longs) is read from chunk 0 rather than from `$F80000`. Emu68 does exactly the same
for a plain 512 KiB image, where it copies the Kickstart to both `$E00000` and `$F80000`; a 2 MB
image has to carry that mirror itself. A blank chunk 0 means the m68k starts at `PC=$ffffffff` and
dies in an exception storm before the Kickstart executes one instruction. `$E00000` is not in
`scanBounds`, so the mirror contributes no romtags and nothing is initialised twice.

Exec only looks for ROM modules where its scan bounds table tells it to, and a stock ROM does not
list `$A80000`. It does list `$F00000`, which a 2 MB image never maps — so
`scripts/kickpatch.py` repoints that one dead entry at the extension bank and fixes the ROM
checksum. Three bytes change in the whole Kickstart; nothing is added, moved or re-pointed.

A small resident in the extension then brings the stack up as part of the normal Kickstart
init sequence, which is what makes the timing work.

Everything lands in a nine-slot window near the bottom of the coldstart chain, and that window is
not a matter of taste. The Emu68 drivers need `devicetree.resource`, `gic400.library`,
`mailbox.resource` and `68040.library` — and those live in Emu68's *own* Z3 board ROM, not in the
Kickstart, so they are not in exec's resident list at all. What puts them there is `romboot` at
priority **−40**: it walks the ConfigDev chain, finds the romtag in the board's diag area and does
`SetCurrentBinding()` + `InitResident()` on it. (`diag init` at 105 is a red herring: it copies each
diag area and calls its DiagPoint, which for the Emu68 board only relocates. It initialises
nothing.) So the Emu68 module window opens at −40, and no module that depends on it can sit above
that. At the other end, `bootmenu` at −50 lists the boot volumes, so everything has to be mounted
before then. `−41…−49` is empty in a stock 3.2 Kickstart, and that is where the ROM set goes:

| Priority | What |
|---|---|
| 110 | `expansion.library` — configures the Emu68 board |
| 105 | `diag init` — copies diag areas and calls their DiagPoint. *Not* the module window |
| 103 | `utility.library` — the only Kickstart library a class `libInit` needs |
| 80 / 50 / 40 | `FileSystem.resource` / `timer.device` / `input.device` |
| +10 | `ODFileSystem` — registers in `FileSystem.resource`, so it must exist before anything mounts. The one deliberate exception to the band; see below |
| **−40** | **`romboot`** — binds the Emu68 board romtag → `devicetree.resource`, `gic400.library`, `mailbox.resource`, `68040.library` |
| −41 | `bcmpcie.library` |
| −42 / −43 | `xhci.device` / `nvme.device` — NVMe probes and mounts here |
| −44 | `poseidon.library` |
| −45 | **every** `*.class` — the five in the shipping image are `hubss`, `hub`, `massstorage`, `bootmouse`, `bootkeyboard` |
| **−46** | **Poseidon ROM Init** — adds the classes and the host controller, scans, hands keyboard and mouse to the boot classes, then *waits* for every hub to finish its port scan and for USB storage to mount |
| −50 | `bootmenu` — lists the boot volumes |
| −60 | `strap` — picks the boot volume |

One resident, not the AROS two. AROS splits this into an "early" (35) and a "late" (28) half
because *there* `input.device` is priority 30 — below the startup resident — so the input classes
had to be pushed underneath it or `hid.class` would bind before the input handler existed. On
AmigaOS 3.2 `input.device` is at 40, so it has been up for most of the coldstart chain by the time
−46 runs, and that race cannot happen. Nothing else separated the halves.

The resident deliberately blocks: exec runs this chain in priority order and `strap` sits at
the bottom of it, so holding at −46 until the mount has happened is what enables "boot from USB".
It holds for two things in turn: every hub having finished its port scan
(`UCM_PortsPending` — a hub still in its power-good wait, or between seeing a connection and
enumerating it, has nothing in the device list yet, so behind a chain of hubs a quiet bus means
the keyboard, mouse and disk are all still to come), and then the media and mounts
(`UCM_MediaPending`). It gives up after a bounded wait, so a machine with no USB storage pays
about a second and a wedged device cannot hang the boot. The input classes are added *before* the
bus is enumerated, so the keyboard and mouse are live during that wait rather than after it.

−45 is the only slot in the band a class can occupy, which is why it is a build-system default
(`PSD_CLASS_PRI`) rather than a per-class choice: it has to be *under* `poseidon.library` (−44)
and strictly *above* the startup resident, because it `psdAddClass()`es the classes by name and
that resolves to an `OpenLibrary()` of an already-initialised library. Equal priority would not do
— exec's tie-break is scan order within the image, so a class sharing −46 with the resident
could be created after the resident that wants it. Every class therefore carries −45, whether or
not it is in the image; it costs nothing, since RamLib reads neither the flags nor the priority of
a `LIBS:USB/` copy. `usbaudio.class` is the single override at −120, out of the band on purpose:
it needs `ahi.device`, so it can never be a coldstart module, and being below −49 makes
`build-kickstart.sh` refuse an image that contains it.

`build-kickstart.sh` refuses to build an image whose modules fall outside `−41…−49`, and warns if
your Kickstart has residents of its own in that range. The failure mode it is guarding against is
silent: above −40 `bcmpcie.library` finds no device tree and no PCIe host bridge, and a `rangeops`
driver reports `Emu68 lacks dcache-range-ops rev 1` — not because the firmware lacks it, but
because there is nothing to ask yet.

Above the band is a warning rather than an error, because it is sometimes right. `ODFileSystem`
at +10 is the case that matters: it only adds itself to `FileSystem.resource` (+80) and has to be
there before `nvme.device` (−43) or `massstorage.class` mounts anything, so it belongs above
`romboot`, not in the band. Anything that touches the device tree, PCIe, MSI or the 68040 cache
patches does not.

## The ROM-clean rule

A ROM module must contain no writable data at all — no `.data`, no `.bss` — and the failure when
it does is quiet rather than loud. `romtool` reserves space for both sections in the image (`.bss`
zero-filled, `.data` with its initial values), so a module with either one still loads and runs,
and its *reads* are correct. It is the writes that vanish: the bank is mapped read-only, so the
store goes out to a bus address nothing answers, and nothing is logged. An image that "works apart
from silently losing state" is not a useful thing to be able to build, so there is no override —
the fix is always to move the state into an allocated struct, and the libbase is its usual home.

This is enforced **where each module is built**, by `psd_rom_check()`
(`cmake/PoseidonRomCheck.cmake`) here and `emu68_rom_check()` in the driver stack: an `objdump -h`
test that fails the build. `build-kickstart.sh` does not re-check, because it has to run on a
user's PC with no m68k toolchain — the build guards are the check, which is also why a `-serial`
archive ships no `ROM/` drawer (`debug.lib` carries a writable `_SysBase`). A module from outside
either build is its builder's responsibility.

## Naming the host controller

The startup resident opens its controller by name, and that name is patchable per image rather
than compiled in: it lives in a fixed-size field behind a cookie in the module
(`ROMSTART_HCD_COOKIE`, `romstartup/usbromstart.c`), and `build-kickstart.sh --hcd <name>` rewrites
it in a copy before the bank is assembled. So embedding a different controller needs no rebuild of
the distribution; `cmake -DROMSTART_HCD=<name>` only sets the default. The name is limited to 31
characters, and the script warns if it matches no module being embedded — nearly always a typo or
a forgotten driver.

**It must be a real Poseidon host controller.** The resident opens the device before DOS exists
and waits on it with no timeout, so a name that resolves to something which is *not* a host
controller hangs the coldstart chain instead of reporting an error — the machine simply never
boots.

What `--hcd` cannot do is disambiguate two drivers that share a name: the context (6.x) and legacy
(5.x) `xhci.device` builds both carry the romtag name `xhci.device` at priority −42, and on disk
they are separated only by directory. A ROM has no directories, so exactly one can be embedded —
which one is decided by the file passed on the command line, and the version in the printed scan
is how you confirm it.

## Three things worth knowing

**There is room to spare, `hid.class` included.** Every class is ROM-clean, so what goes in the
1 MB extension is a per-build choice rather than a capability question. The shipping set uses
`bootmouse`/`bootkeyboard` and leaves roughly 295 KiB free — enough that `hid.class` fits instead,
and the resident already prefers it over the boot classes when it is present.

**The extension is not in Emu68's ROM fast path.** Emu68 treats `$F80000..$FFFFFF` as ROM for
translation purposes and `$A80000` is outside that. The region is mapped read-only so this is
correct, but it is worth knowing if you go looking at translation behaviour.

**How Early Startup Control works from USB.** The 3.2 boot menu never reads the joystick port: it
opens `input.device`, waits four frames and samples `PeekQualifier()` once. So the boot classes
inject with `IND_ADDEVENT`, the V47 command that also updates input.device's own qualifier state
(NDK `input.doc`), and ask the device for a finite idle rate at bind, so a mouse or keyboard
already held reports its state within 100 ms although nothing changed — at idle rate 0 a device
stays silent until something changes, and `Get_Report(Input)` is answered blank by much firmware
— then go back to report-on-change after that first report. Two limits: the HELP-key entry reads
`keyboard.device`'s key matrix, which only a keyboard on the Amiga's own connector reaches; and the
device has to be bound before the boot gate releases — which the gate guarantees, since it holds
until every hub has finished its port scan.

## Building it from a source checkout

`scripts/build-kickstart.sh` is the same script the archive ships; run out of the repository it
takes the Poseidon modules and the startup resident from the build tree instead of from `Libs/`
and `Classes/USB/`:

```
./build.sh --build                        # any backend but serial
./scripts/build-kickstart.sh [--hcd <name>] <kick.rom> <bcmpcie.library> <xhci.device> [module ...]
```

`BUILD_DIR` (default `build/`) selects the tree, `OUT` the image (default
`build/kick-usb-2m.rom`), `KICK` the Kickstart if not given as an argument. It still needs
`romtool` (`pipx install amitools`), and it no longer needs an m68k toolchain. The `ROM/` drawer
is produced by the `package` target, which omits it for a serial build.
