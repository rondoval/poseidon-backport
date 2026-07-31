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

## Before you install

**This is not a driver for your USB card.** Poseidon is the stack that sits above the
hardware; it needs a *USB host-controller driver* to talk to, and none is included. This
release speaks two interfaces to such a driver — see *[Two host-controller
interfaces](#two-host-controller-interfaces)* below — and both are confirmed working
against the [Emu68 driver stack](https://github.com/rondoval/emu68-driver-stack)
(PiStorm / Emu68 on a Raspberry Pi 4 or CM4). Take its `xhci.device` **6.x** if you can:
`xhci.device` **5.x** works too, with fewer features.

**It replaces an existing Poseidon.** Installing puts `poseidon.library` in `LIBS:` and
the class drivers in `SYS:Classes/USB/`, over whatever is there now. The installer
version-checks every file and never replaces a newer one without asking, but a classic
Poseidon 4.x installation will end up being upgraded. Your settings are kept: they live
in `ENVARC:Sys/poseidon.prefs` as before, and this release reads the classic file format
(AROS had changed the file's identifier; it is changed back).

**The classes in this archive need `poseidon.library` 6.** They will not load against a
4.x or 5.x library, so install the whole archive rather than picking pieces out of it.

**68040 or 68060 with FPU.** The released binaries are built for `-m68040 -mhard-float`
and will not run on a 68000/010/020/030. Building from source for another CPU is a
one-line change.

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

---

## Known limitations

- **BOT read throughput.** On drives that only speak the older BOT transport, sustained
  reads run below what the drive can manage. The cause is understood (unaligned transfer
  buffers being copied wholesale on the driver side) and the fix is in progress. UAS
  drives are not affected.
- **MUI 5 is required for any settings window** — Trident and the per-class configuration
  dialogs. The stack itself runs without MUI; you just cannot configure it from a GUI.
- **Not tested with classic Amiga USB cards.** The legacy interface itself is confirmed
  working — `xhci.device` 5.x runs on it — but no Deneb, Subway or similar card has been
  tried, so the classic cards remain untested in practice.
- **No ROM version yet.** The stack is built to be ROM-able and every component is
  verified free of writable data, but assembling it into a Kickstart-replacement ROM (so
  USB comes up from cold boot) is still to come.

---

## What's in the archive

Two archives are published; take the first unless you are chasing a problem.

| Archive | When to use it |
|---|---|
| `Poseidon-6.0.lha` | **Start here.** The normal release build. |
| `Poseidon-6.0-serial.lha` | Identical, but the stack, classes and tools also print diagnostics to the serial port. Troubleshooting only. |

Unpack on the Amiga and run the `Install` script. It installs the library, the 29 class
drivers, the five shell commands, Trident with its icon and translations, the USB
attach/detach sounds and the preset datatype, optionally the per-gadget tools, and can
add the three startup commands to `S:User-Startup` so USB is up at boot. Reboot
afterwards.

---

## Requirements

- **AmigaOS 3.2**
- A **68040 or 68060 with FPU**
- A **USB host-controller driver** (not included) — `xhci.device` **6.x** from the
  [Emu68 driver stack](https://github.com/rondoval/emu68-driver-stack) for the full
  feature set, or `xhci.device` **5.x** / a classic USB card for the legacy one
- **MUI 5** for Trident and the class settings dialogs

---

## Credits and licence

- **Chris Hodges** — original author of Poseidon (2002–2009).
- **The AROS Development Team** — maintainers of the 5.x line since 2009.

Poseidon for AmigaOS is distributed under the **AROS Public License (APL) Version 1.1**,
the same licence Chris Hodges released the sources under. A few components carry their
own (an ISC-licensed driver port, a BSD-licensed submodule, GPL-licensed icon artwork);
see [LEGAL](LEGAL) for the full attribution. MUI is a runtime dependency and is not part
of this distribution.
