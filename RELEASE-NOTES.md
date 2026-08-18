# Release notes — Poseidon for AmigaOS 6.1

Everything in this archive — `poseidon.library`, all 29 class drivers, Trident, USBEject
and the command-line tools — reports version **6.1**, followed by the CPU it was built
for.

**Upgrading from 6.0:** install the whole archive. Safe eject adds a `poseidon.library`
function that `massstorage.class`, Trident and USBEject call, and a new class against an
old library would guru. Your settings are untouched.

## An archive for your CPU

6.0 shipped one `-m68040 -mhard-float` build, which a 68020 or 68030 could not run at all.
6.1 ships **three archives — `-020`, `-040` and `-060`**, each with the usual `-serial`
diagnostic counterpart, and **none needs an FPU**. The stack contains no floating point at
all; the only code in the distribution that does is the gamma table in the optional
`PencamTool`/`SonixcamTool` — soft-float in `-020`, covered by
`68040.library`/`68060.library` on an FPU-less 040 or 060. The `-020` archive runs on all
three CPUs, just untuned for the faster two.

All three carry the **same version number** — it is the library's ABI version, not the build
variant — so every component appends its CPU to its version string: `Version
LIBS:poseidon.library` reports `poseidon.library 6.1 (…) Poseidon for AmigaOS 68040`.

**Switching CPU variant needs the version requester.** Installing one variant over another
is a same-version copy, which the installer's version check skips. Run the Installer at the
*Average* or *Expert* user level, where it offers to overwrite, or delete the installed
files first.

## Safely remove hardware

USB drives can now be ejected cleanly before you unplug them. **USBEject** (optional,
`SYS:WBStartup/`) keeps an *Eject* item per drive in its own **USB** menu on Workbench;
Workbench replacements emulate only the flat AppMenu, so the items land in **Tools**
instead (under **Directory Opus 5**, switch *Show Tools menu* on). Trident's Devices page
has an **Eject** button that needs no Workbench at all. Both are localized in the usual six
languages.

An eject flushes every volume and asks each filesystem to inhibit. **A filesystem that
reports a volume still in use vetoes the whole eject, and the requester names it** — nothing
is unmounted, so there is no half-ejected state. Otherwise the write cache is synced, the
drive stopped and its hub port switched off, and a requester confirms it is safe to pull.

Two things it cannot see. A filesystem that does not implement `Inhibit()` — some CD
handlers — cannot report itself busy, so the eject proceeds once its buffers are flushed;
the error log says so when this happens. And raw access below the filesystem, such as a
backup or imaging tool on `usbscsi.device`, is invisible to it, exactly as on other systems.
Close your files before ejecting a disc.

### For developers

Safe eject is a first-class stack operation, laid out like suspend/resume:

| | |
|---|---|
| `psdSafeEjectDevice(pd, busybuf, len)` | Eject a device: every bound class that can, then off the bus. Returns `SAFEEJECT_OK` / `_BUSY` (busy object named in `busybuf`) / `_FAIL` / `_NOT_SUPPORTED`. From a Process, without a device lock — it blocks as long as the filesystems and hardware need. |
| `DA_CanSafeEject` | Read-only device attribute: anything here to eject? What the menu and the button grey themselves on. |
| `UCM_SafeEject` + `UCCA_SupportsSafeEject` | The class side: advertise the capability, then quiesce all-or-nothing whatever you hold, refusing with `SAFEEJECT_BUSY` rather than lose data. Only `massstorage.class` implements it today. |

## MUI 3.8 is enough

6.0 required MUI 5 for Trident and the per-class settings dialogs. They now open on
**`muimaster.library` 19 and up** — MUI 3.8, MUI 4.0 and MUI 5 alike. Nothing in the GUIs
was given up for it: every MUI attribute, method, class and library function they use is
checked against the MUI 3.8 headers on every build, and the newest thing any of them
touches is `MUIM_Application_AboutMUI`, from muimaster 14.

## Kickstart 3.1 is enough

The floor is now **Kickstart and Workbench 3.1**, whole distribution. Five changes:

- `hid.class` sent every keystroke and mouse movement with `IND_ADDEVENT`, `input.device` V47.
  Below 3.2 each was rejected with `IOERR_NOCMD` and dropped, unlogged — **HID keyboards and mice
  did not work at all**. It now tests the device version and falls back to `IND_WRITEEVENT`.
- Trident opened `icon.library` 44 and USBEject `workbench.library` 45.
- `hid.class` patched `SetJoyPortAttrsA` (`lowlevel.library` V40.27) without checking the jump
  table reached it, corrupting memory on an older 40.x. It now tests `lib_NegSize` first.
- `hid.class` abandoned its whole action engine if `datatypes.library` or `commodities.library` was
  missing. Both are optional now.
- `Install` tests for Installer 43.3 up front, which its `foreach` loops have always needed.

Differences below 3.2: see **Known limitations** in `README.md`.

## A name and buffer count per filesystem

Mass storage used to apply one DOS name and one buffer count to everything it mounted, so a
CD came up as `UMSD3` among your USB sticks with hard-disk buffering. The *LUN Settings* page
now carries a *Mount name and buffers* row per filesystem — FAT, NTFS, exFAT, CD/DVD — and,
like everything there, **per LUN**, so each slot of a card reader can be named separately.
Out of the box discs mount as `UCD0` with 25 buffers, sticks stay in the `UMSD0…` sequence
with 100, and *Save as Default* sets that starting point for drives with none of their own.
RDB partitions are unaffected.

**Upgrading:** your existing name and buffer count carry over to *every* filesystem, so
mounts come up where they always did. Change the CD row if you want the new `UCD*` pool.

## exFAT sticks mount

Modern USB sticks — anything above 32 GB, and most of what you buy preformatted — are exFAT,
and mass storage previously recognized them only well enough to skip them. They now mount as
a superfloppy, from an MBR partition or from a GPT one, identified by their own boot sector,
so a stick mislabelled as NTFS (type `0x07` means either) still lands on the right handler.

**Two free downloads this archive does not contain are required:** `exFATFileSystem` in `L:`
(relan's libexfat, ported by Fredrik Wikström, 68k branch by Tobias Karlsson; read *and*
write) and `filesysbox.library` 53 or newer in `LIBS:`, which it runs on — a 68020 binary, so
exFAT needs an 020 or better. Both are configured out of the box; without them exFAT media
are skipped. Clearing a handler row is now the off switch for any filesystem.

**Eject exFAT drives before unplugging** — the handler writes a volume-clean flag on its way
out, and pulling the stick first leaves it marked dirty.

## Every disc ODFileSystem can read

A disc used to be refused unless it carried an ISO 9660 volume descriptor — all the old
`CDFileSystem` can read. Mass storage now recognises **ODFileSystem** by name and lets it
identify discs itself, so **High Sierra, UDF, HFS and HFS+** mount alongside ISO 9660 (Joliet
and Rock Ridge included), and an **audio CD** mounts with its tracks as playable WAV files.
Amiga-bootable and RDB-formatted discs are still recognised first, an unreadable disc is
still refused, and any other handler keeps the ISO-only check.

**ODFileSystem is not in this archive** — it is a free download (Stefan Reinauer's, BSD).
Fresh installs set the CD/DVD *DosType* to `CD01`, its own, and the configured handler now
takes precedence over an older CD filesystem in a controller ROM. Existing settings keep
working.

---

# Release notes — Poseidon for AmigaOS 6.0

The first release of **Poseidon for AmigaOS** — the Poseidon USB stack, back on the
machine it was written for, and taken a good deal further while it was at it.

Poseidon was Chris Hodges' USB stack for AmigaOS (2002–2009). In 2009 he placed the
sources into AROS, and the AROS Development Team carried them forward for the next
seventeen years — gaining a SuperSpeed hub class, more class drivers and a long tail of
fixes, none of which an AmigaOS machine could use. This project takes that evolved
version (AROS commit `c01498ab`) and brings it home, rebuilt for **AmigaOS 3.2** on 68k.

**The port is only half of it.** Roughly as much of this release is new work that has no
counterpart in the AROS line at all: a second, purpose-built interface to the host
controller and the reworked SuperSpeed handling that rides on it, UAS mass storage over
bulk streams with several commands in flight and real error recovery, USB link power
management under your control, a rebuilt mounting layer, and the isochronous-audio and
suspend fixes. Everything below is the delta against that AROS baseline, and each section
says plainly whether it is new here or a port of something that was already there.

Everything in this archive — `poseidon.library`, all 29 class drivers, Trident and the
command-line tools — reports version **6.0**.

---

## What's new since the AROS 5.x line

### It runs on AmigaOS 3.2

**This is the port half.** The whole distribution was rebuilt as a native AmigaOS program
set: the AROS build system is gone, and with it the parts that only make sense on AROS —
AROS's own USB host controllers, the hosted virtual controller, and the Allwinner-specific
`felsunxi` class. What remains is 29 class drivers, the library, Trident and five shell
commands, plus an Installer script, icons, a datatype so Poseidon preset files show their
own icon, and Trident's translations (Czech, French, Italian, Polish, Russian, Spanish).

### Two host-controller interfaces

**New here.** Poseidon has always talked to a host-controller driver by filling in a
`struct IOUsbHWReq` and sending it — the classic V1/V2 request. It describes a transfer
the way a 1998 controller wanted one: an address, an endpoint, a buffer. A USB 3.0
controller wants something quite different. It keeps *contexts* — durable per-device and
per-endpoint state it is told once — and a transfer is then a tiny reference to those
contexts. Handed only classic requests, an xHCI driver has to reconstruct the topology by
watching the traffic go past and guessing what the stack is doing.

AROS's answer was to make the request bigger: a **"V3" extension** that bolted the missing
topology onto *every* transfer — route string, root port, split/TT parameters, SuperSpeed
burst settings. It never existed on AmigaOS, and it puts once-per-device facts on the hot
path of every single transfer. **It has been removed here**, and replaced with a **context
interface**: a set of operations that establish a device and its endpoints once, up front,
so the transfers themselves stay small. It is what `xhci.device` 6.x speaks, and where
everything below in this section comes from.

**The legacy interface stays, and stays frozen.** The classic V1+V2 `IOUsbHWReq` layout
and every `UHCMD_*`/`UHIOERR_*` value are untouched binary contract, because classic Amiga
USB cards — Deneb, Subway and friends — depend on them. The stack keeps a complete second
backend for that path; it is not a compatibility stub.

Both are confirmed working: **`xhci.device` 6.x** on the context interface, and
**`xhci.device` 5.x** on the legacy one. You tell the stack which driver to open, and it
asks that driver which interface it implements. On the legacy interface you get the
classic feature set — USB 2.0-style device handling, no `hubss.class` binding (so no
SuperSpeed hubs), no UAS bulk streams, and whatever link power management the driver
chooses to do on its own, with no control from Trident.

### USB 3.0 that behaves like USB 3.0

**New here**, on the context interface. A SuperSpeed device now reaches the stack *as* a
SuperSpeed device instead of being disguised as USB 2.0 on the way in, and the SuperSpeed
hub driver was largely rewritten around that: real SuperSpeed port status handling, warm
reset, and no leftover low/full-speed machinery that a USB 3 hub never needs.

**A USB 3 drive no longer shows up twice.** A USB 3 hub presents itself as two hubs — a
USB 2.0 half and a SuperSpeed half — and a SuperSpeed device appears on both. The two
halves are now recognised as one hub (they share a Container ID) and the duplicate is
dropped. The same race could also hang the machine while a device was being plugged in;
that is fixed too.

### Mass storage: UAS, and much faster

**New here.** The AROS line had the outline of a UAS transport; it is now a working one.
Several commands run on the drive at once over USB 3.0 bulk streams, with a **queue
depth** you can set in the settings, and UAS is preferred over the older BOT transport
when a drive offers both (the preference is a switch, so you can force BOT). Bulk streams
need the context interface; on the legacy one UAS falls back to one command at a time.

The class also stopped getting in its own way: reading geometry, mode pages, `HD_SCSICMD`
passthrough and seeks used to stall all block I/O while they ran, and no longer do. NAK
timeout handling was reworked, which mostly matters to CD and DVD drives that take their
time.

**Recovery instead of a hang.** When a command wedges, the class now aborts just that
command with a UAS task-management request; if the drive ignores that, it resets the
device. Previously a stuck command took the drive — and sometimes the machine — with it.

### Mounting, rewritten

**New here.** Every kind of medium is now mounted through the A4091 mounter (the partition
parser from `a4091.device`), which handles **RDB, MBR, GPT and superfloppy** disks from
one place. In the mass-storage settings you choose which handler to use for **FAT**,
**NTFS** and **CD/DVD** partitions, and audio CDs are recognised as such. The automount
switches — RDB, MBR/GPT, CD/DVD, and unmounting on removal — are all per-category.

### USB power management

**New here**, on the context interface: a **link power management** switch that lets an
idle link drop into a low-power state between transfers (U1/U2 on SuperSpeed, L1 on
High-Speed) and enables Latency Tolerance Messaging. Entry and exit are handled by the
controller and are invisible to transfers; turn it off if a device misbehaves when idle.
It applies immediately, both ways, and can be overridden per device. (A legacy-interface
driver may do link power management of its own accord — `xhci.device` 5.x does — but the
stack has no say in it and Trident's switch does nothing there.)

Poseidon's older power-saving suspend got a working-over alongside it: root hubs can now
be suspended, and remote wakeup is always armed before a device is put to sleep, so a
suspended keyboard or mouse brings its own link back up the moment you use it instead of
needing a replug. Devices that should not sleep — a mounted drive, an active HID device —
no longer do. Unplugging a device *while it was suspended* used to leave it lingering in
the stack; that is fixed.

### USB audio keeps playing

Rescanning the class drivers used to tear down isochronous transfers, which meant audio
stopped whenever the stack rescanned. The scan was reworked so it does not. A wrong
endpoint address the audio class sent on the wire — which `xhci.device` used to carry a
workaround for — was fixed at the source.

### "Use plain, factual messages"

**New here.** Poseidon traditionally reports itself with a certain amount of humour. In Trident,
*Various Settings → Logging Options → **Use plain, factual messages*** swaps every log
entry, popup and requester for a short factual one. It is **off** by default, so the
traditional wording is what you get unless you ask for otherwise, and it takes effect
immediately for new messages.

### Other fixes

- The HID class settings window no longer crashes when opened.
- Hubs cope with a device disappearing mid-operation instead of reporting a string of
  spurious errors.
- After a resume, hubs no longer submitted the same status-change request twice.
- Unknown hubs and devices get a sensible name in Trident instead of a blank one.
- A `TD_SEEK`/`TD_SEEK64` on a mass-storage unit used to send an uninitialised command to
  the drive.
