# Poseidon for AmigaOS

The USB stack for **AmigaOS 3.1 and newer**. Plug in a keyboard, a mouse, a memory stick, a
USB sound card or a network adapter and use it from Workbench — hot-plug, unplug, no reboot.

Poseidon was written for AmigaOS by **Chris Hodges** (2002–2009). In 2009 he placed the
sources into AROS, where the **AROS Development Team** maintained and extended them for
the next seventeen years. This project brings that evolved version back to its original
platform — and then goes a good deal further than it.

**This is more than a port.** A second way of talking to the host controller, largely
rewritten SuperSpeed hub handling, UAS mass storage over bulk streams with several
commands in flight, USB link power management and a rebuilt mounting layer are all new
work done here, on top of what the AROS line provides.
[`RELEASE-NOTES.md`](RELEASE-NOTES.md) is the itemised list.

## What you get

- **Real plug and play** — devices appear and disappear as you connect them.
- **29 USB class drivers** covering input, storage, networking, serial, printing, audio
  and MIDI (full list [below](#class-drivers)).
- **USB 3.0 SuperSpeed** — SuperSpeed hubs and devices are handled as SuperSpeed, and
  mass storage uses UAS with several commands in flight at once.
- **USB power management** — idle devices and idle links drop into low-power states, and
  a suspended keyboard or mouse wakes its own link the moment you use it.
- **Trident** — a control panel where you see every connected device, configure each
  class, and suspend, resume or power-cycle a port by hand.
- **Shell commands** — start the stack, list what is attached, read the error log.

**What you also need, and is not in here:** a *USB host-controller driver* — the piece
that talks to your actual USB hardware. Poseidon sits above it. See
[Talking to your USB hardware](#talking-to-your-usb-hardware).

## Requirements

- **Kickstart and Workbench 3.1, or newer** — the whole distribution, Trident and USBEject
  included. A few details differ below 3.2; see [Known limitations](#known-limitations).
- **Installer 43.3 or newer** to run the `Install` script — it is freely distributable and is
  on Aminet as `util/misc/Installer-43_3.lha`. OS 3.5 and later already have one.
- A **68020 or better**, and **no FPU is required**. The release ships one archive per
  CPU — `-020`, `-040` and `-060` — so take the one that matches your machine:

  | Archive | For | Built with |
  |---|---|---|
  | `-020` | 68020 and 68030 | `-m68020 -msoft-float` |
  | `-040` | 68040, and PiStorm / Emu68 | `-m68040 -mhard-float` |
  | `-060` | 68060 | `-m68060 -mhard-float` |

  The `-020` archive also runs on an 040 or 060, just not tuned for them. On the FPU:
  the stack itself — `poseidon.library`, the class drivers, Trident and USBEject —
  contains no floating point at all. The only floating point in the distribution is the
  gamma table computed by the two optional camera tools, `PencamTool` and
  `SonixcamTool`; in the `-020` archive that is soft-float, and in the `-040`/`-060`
  archives `68040.library`/`68060.library` cover it on a part without an FPU.
- A **USB host-controller driver** — see [below](#talking-to-your-usb-hardware). Both
  `xhci.device` **6.x** and `xhci.device` **5.x** from the
  [Emu68 driver stack](https://github.com/rondoval/emu68-driver-stack) (PiStorm / Emu68
  on a Raspberry Pi 4 or CM4) are confirmed working.
- **MUI 3.8 or newer** (`muimaster.library` 19+), for Trident and the per-class settings
  dialogs; the stack itself runs without it.
- A **640×480 or larger screen**, again only for the GUI. Trident's window does not fit a
  shorter one — 640×256 PAL or 640×200 NTSC — and says so rather than opening: *"Couldn't
  open window! Maybe screen is too small. Try a higher resolution!"*

## Talking to your USB hardware

Poseidon does not drive USB controllers itself — a separate host-controller driver does,
and you tell the stack which one to use (`AddUSBHardware xhci.device 0`). This release
can speak **two different interfaces** to that driver, and which one your driver
implements decides what the stack can do with it.

| | **Legacy interface** | **Context interface** |
|---|---|---|
| Who speaks it | Classic Amiga USB cards (Deneb, Subway, …) and `xhci.device` **5.x** | `xhci.device` **6.x** |
| USB 2.0 and older devices | yes | yes |
| SuperSpeed devices | yes, but the stack sees them as USB 2.0 | handled as SuperSpeed |
| SuperSpeed (USB 3) hubs | no — `hubss.class` declines to bind | yes |
| UAS mass storage | one command at a time (no bulk streams) | bulk streams, several commands in flight |
| Link power management | whatever the driver does on its own, with no control from Trident | the stack's policy — switchable in Trident, overridable per device |

**Both are confirmed working.** The legacy interface is the classic
`IOUsbHWReq`/`UHCMD_*` contract Poseidon has always used, unchanged and frozen, so
existing third-party drivers keep working exactly as they did — you simply get the
classic feature set. The context interface is new in 6.0 and is where the USB 3.0 work
lives; take `xhci.device` 6.x if your hardware has one.

Having named the driver, you do not have to say anything further: the stack asks it which
interface it implements and uses that one.

## Download

Take the archive from the
[Releases](https://github.com/rondoval/poseidon-backport/releases) page. Pick the one for
your CPU (see [Requirements](#requirements)) — that is the only choice that matters:

| Archive | When to use it |
|---|---|
| `Poseidon-<ver>-020.lha` | **Start here** on a 68020 or 68030. |
| `Poseidon-<ver>-040.lha` | **Start here** on a 68040, and on PiStorm / Emu68. |
| `Poseidon-<ver>-060.lha` | **Start here** on a 68060. |
| `Poseidon-<ver>-<cpu>-serial.lha` | Identical to the above, but also prints diagnostics to the serial port. Troubleshooting only. |

The archives are otherwise the same: same version, same components, same settings. Every
component reports its CPU at the end of its version string, so `Version
LIBS:poseidon.library` on the running machine tells you which one is installed.

## Installing

1. Unpack the archive on your Amiga.
2. Run the `Install` script (double-click its icon, or `Installer Install` from a Shell).
3. Answer the questions, then reboot.

The installer copies `poseidon.library` to `LIBS:`, the class drivers to
`SYS:Classes/USB/`, the shell commands to `C:`, and Trident with its icon, translations
and the USB attach/detach sounds to `SYS:Prefs/`. It version-checks everything and never
replaces a newer file without asking. The safe-eject Workbench menu (USBEject), the
optional per-gadget tools and the Poseidon preset datatype are separate questions.

**Switching CPU variant.** All three CPU archives carry the same version number — that
number is the library's ABI version, not the build variant — so installing one over
another is a same-version copy, which the installer's version check would otherwise skip.
Run the installer at the *Average* or *Expert* user level, where it shows the version
requester and lets you overwrite, or delete the previously installed files first.

It also offers to add the three startup commands to `S:User-Startup`, which is what you
want unless you prefer to start the stack yourself:

```
PsdStackLoader
AddUSBHardware xhci.device 0     ; your host-controller device and unit
AddUSBClasses
```

If you already run a Poseidon, this upgrades it. Your settings are kept — they live in
`ENVARC:Sys/poseidon.prefs` as they always did, in the classic Poseidon file format (AROS
had changed the file's identifier; it is changed back).

Note that the classes and tools in this archive require `poseidon.library` **6**, so
install the whole archive rather than picking pieces out of it.

## Settings worth knowing about

Open **Trident** (`SYS:Prefs/Trident`). Beyond the device list, a few switches are worth
finding:

- *Various Settings → Stack Settings → **Enable power-saving suspend mode*** puts idle
  devices to sleep. A suspended keyboard or mouse brings its own link back up the moment
  you use it.
- *Various Settings → Stack Settings → **Enable USB link power management*** lets an idle
  link drop into a low-power state between transfers (U1/U2 on SuperSpeed, L1 on
  High-Speed). The controller handles it invisibly; turn it off if a device misbehaves
  when idle. It can also be overridden per device. Needs a driver that speaks the context
  interface — on a legacy driver the switch does nothing.
- *Various Settings → Logging Options → **Use plain, factual messages*** replaces
  Poseidon's traditional light-hearted log entries, popups and requesters with short
  factual ones. Off by default.
- **massstorage settings** (select the class, then *Configure*) — which handler to use
  for FAT, NTFS, exFAT and CD/DVD partitions; what to automount (RDB, MBR/GPT, CD/DVD) and
  whether to unmount on removal; whether to prefer UAS over the older BOT transport, and
  the UAS queue depth. The DOS name and buffer count are set **per filesystem**, so discs
  mount as `UCD0` with CD-sized buffering while sticks stay in the `UMSD0…` sequence —
  give two filesystems the same name and they simply share one sequence. **exFAT** is
  configured out of the box but needs two files this archive does not contain:
  `exFATFileSystem` in `L:` and `filesysbox.library` in `LIBS:`. Without them exFAT media
  are skipped, exactly as before — and clearing a handler row is how you turn any
  filesystem off.
- **The device list** — suspend, resume, power-cycle or safely eject a device by hand.
  Power-cycling often revives a device that has stopped responding. *Eject* flushes and
  unmounts every volume on a mass-storage device (refusing while files are open on it)
  and then disables its port, so it can be unplugged without losing data.

## Class drivers

| Category | Classes |
|---|---|
| **Hubs** | `hub`, `hubss` (USB 3 SuperSpeed) |
| **Human input (HID)** | `bootmouse`, `bootkeyboard`, `hid` (full HID parser), `egalaxtouch` (touchscreen) |
| **Mass storage & imaging** | `massstorage` (BOT / CBI / UAS), `ptp` (cameras), `dfu` (firmware upgrade), `rawwrap` |
| **Serial & modem** | `cdcacm`, `serialpl2303` (PL2303), `serialcp210x` (CP210x) |
| **Printer** | `printer` (provides `usbparallel.device`) |
| **Networking (SANA-II)** | `cdceth`, `asixeth` (ASIX), `pegasuseth` (Pegasus), `davicometh` (DM9601), `moschipeth` (MosChip), `ethwrap`, `rndis`, `lan78xx` (Microchip LAN78xx) |
| **MIDI & audio** | `simplemidi`, `camdmidi` (CAMD), `audio` (USB audio → `ahi.device`) |
| **Other** | `bluetooth`, `stir4200` (IrDA), `palmpda`, `arosx` (Xbox gamepad) |

## Tools

Shell commands, installed to `C:`:

- **PsdStackLoader** — brings the stack up.
- **AddUSBHardware** — attaches a host-controller device (e.g. `xhci.device`).
- **AddUSBClasses** — loads the class drivers.
- **PsdDevLister** — lists connected USB devices (`lsusb`-like).
- **PsdErrorlog** — prints the stack's error log.

**USBEject** (optional) installs to `SYS:WBStartup/` and adds a **USB** menu to the
Workbench menu bar with an *Eject* item per attached USB drive — the Amiga's "Safely
Remove Hardware". Ejecting flushes and cleanly unmounts every volume on the drive
(refusing if files are still open, and naming the volume in use), stops the drive and
disables its port; a requester then confirms it is safe to unplug. Note that only
filesystem-level use is seen — a program accessing `usbscsi.device` directly (e.g. a
raw backup tool) cannot veto an eject.

Under a Workbench replacement the drives are offered as plain items in the **Tools**
menu instead of under their own *USB* title, because those hosts emulate the flat
AppMenu only. With **Directory Opus 5** as your desktop, enable *Show Tools menu* in
its Environment display settings to see them. USBEject reports which menu it landed
in — and says so if there is no menu to add to — in the Poseidon error log, viewable in
Trident. Trident's *Eject* button needs no Workbench at all and always works.

Optional per-gadget tools (`DRadioTool`, `PencamTool`, `SonixcamTool`, `RocketTool`,
`PowManTool`, `UPSTool`) install to `SYS:Tools/`.

## Known limitations

- **BOT read throughput.** On drives that only speak the older BOT transport, sustained
  reads run below what the drive can manage. The cause is understood — unaligned transfer
  buffers being copied wholesale on the driver side — and the fix is in progress. UAS
  drives are not affected.
- **Not tested with classic Amiga USB cards.** The legacy interface itself is confirmed
  working — `xhci.device` 5.x runs on it — but no Deneb, Subway or similar card has been
  tried.
- **No ROM version yet.** The stack is built to be ROM-able and every component is verified
  free of writable data, but assembling it into a Kickstart-replacement ROM, so USB comes up
  from cold boot, is still to come.
- **Below OS 3.2, a held key on a USB keyboard does not auto-repeat.** `input.device` gained
  the command that drives its repeat state machine (`IND_ADDEVENT`) in V47; below that,
  events are handed over the older way, which delivers every keystroke and every mouse
  movement but does not repeat. The boot keyboard and boot mouse classes have always worked
  this way, on every OS version.
- **Below `workbench.library` 45, USBEject's entries sit flat in the Tools menu** rather than
  under a **USB** title of their own, because submenus under an AppMenu title need that
  version. The entries themselves, and ejecting, are unaffected. (The same fallback is what
  you get under Directory Opus.)
- **On a `lowlevel.library` older than 40.27, USB gamepads have no analogue stick or rumble.**
  They still work as ordinary joystick and CD32 controllers; it is the analogue/rumble
  extension that needs `SetJoyPortAttrs()`, which that library does not have. Poseidon says
  so in its error log and leaves the library alone.
- **Two HID features want libraries from the Workbench disks, not ROM:** the sound actions
  need `datatypes.library` and the key-string actions need `commodities.library`. If either is
  missing, that feature is skipped with a line in the error log and everything else — keyboard
  and mouse included — carries on as normal.

## Version numbers

`poseidon.library` keeps its name — every USB class and application opens it by that
name, and that compatibility *is* the point — so the version number is what tells this
line apart from the ones before it. Chris Hodges' classic AmigaOS Poseidon is the **4.x**
line and the AROS one is **5.x**; **Poseidon for AmigaOS is 6.x**, and does not track
AROS's numbering.

Every shipped component carries the same version — **6.1** in this release — and
identifies itself as `Poseidon for AmigaOS` in its `$VER` string. Because the 6.x jump
table extends the classic one, the classes and tools require `poseidon.library` **6** or
newer. Host-controller drivers are a separate matter: they are negotiated by capability,
never by version number, so a driver is never gated on a marketing number.

[`RELEASE-NOTES.md`](RELEASE-NOTES.md) lists what changed in each version.

## Licence

Poseidon is distributed under the **AROS Public License (APL) Version 1.1** — see
[LICENSE](LICENSE) for the full text. Chris Hodges placed the original Poseidon sources
into AROS under the APL in 2009.

Some components carry their own licences (an ISC-licensed driver port, a BSD-licensed
submodule, GPL-licensed icon artwork), and MUI is a runtime dependency that is not part
of this distribution. See [LEGAL](LEGAL) for the full attribution and third-party
notices.

## Credits

- **Chris Hodges** — original author of Poseidon (2002–2009).
- **The AROS Development Team** — maintainers of the 5.x line since 2009.
- The third-party authors named in [LEGAL](LEGAL).

---

## Building from source

The easy path needs only **docker** — `./build.sh` runs the build inside the shared
toolchain container (a public image, pulled automatically; no host toolchain to set up):

```sh
./build.sh --build                 # build everything in the container
./build.sh --package               # …and produce the installable Poseidon-<ver>-<cpu>.lha
./build.sh --package --all-cpus    # …one archive per released CPU (020, 040, 060)
```

`BACKEND=pistorm|serial|off` and `DEBUG=<level>` select the debug build; `CPU=68020|68040|68060`
selects the target CPU (default `68040`, and `FPU=` follows it — see
[below](#native-build-without-the-container)). Each non-default CPU gets its own build
tree (`build-020/`, `build-060/`) so the variants never clobber each other. With no flags
`./build.sh` also uploads to a live Amiga — see [CONTRIBUTING](CONTRIBUTING.md).

Under the hood `build.sh` (and CI) run the container build through
`scripts/docker-build.sh`, which owns the image tag and the `cmake` configure
incantation. Drive it directly for a bare build:
`POSEIDON_CONFIGURE_ARGS="-DPOSEIDON_DEBUG_BACKEND=off" ./scripts/docker-build.sh`.

### Native build (without the container)

With a local m68k-amigaos toolchain you can drive `cmake` directly. You need an
**`m68k-amigaos` GCC 16.1** at `/opt/m68k-amigaos` with the **NDK 3.2** headers (provides
`sfdc`), the **MUI 5 SDK**, the **NDK 3.2 SANA+Roadshow** package (the ethernet classes'
`<devices/sana2.h>`), **flexcat** and **python3** on `PATH`, and **CMake ≥ 3.14**. The
MUI/SANA SDKs default to `$HOME/amiga/{MUI5,NDK3.2R4}`; override `AMIGA_SDK_ROOT` (or
`MUI_INCLUDE_DIR` / `SANA2_INCLUDE_DIR`) if yours live elsewhere.

```sh
# the toolchain file is REQUIRED on first configure
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain.cmake
cmake --build build -j"$(nproc)"
cmake --build build --target bootmouse_class    # …or a single component
cmake --build build --target package            # …or build/Poseidon-<ver>-<cpu>.lha
```

**Clean rebuild:** `rm -rf build`, then re-run the configure command (re-passing
`-DCMAKE_TOOLCHAIN_FILE` once the cache is gone).

**CPU** is a pair of configure options, `-DM68K_CPU=` and `-DM68K_FPU=`. The three
released variants are:

| Variant | Configure |
|---|---|
| 020 | `-DM68K_CPU=68020 -DM68K_FPU=soft` |
| 040 (default) | `-DM68K_CPU=68040 -DM68K_FPU=hard` |
| 060 | `-DM68K_CPU=68060 -DM68K_FPU=hard` |

A configured build tree is tied to one CPU, so give each variant its own — which is what
`build.sh` does: `CPU=68060 ./build.sh --package` builds into `build-060/`, and
`./build.sh --package --all-cpus` sweeps all three in one go.

### Project layout

| Path | Contents |
|---|---|
| `poseidon.library/` | The stack core. |
| `usbclass.library/` | The base meta-class every class inherits (ABI headers). |
| `classes/` | The 29 USB class drivers. |
| `trident/` | The MUI control panel + translations. |
| `c/`, `tools/` | CLI commands and optional gadget tools. |
| `include/` | Public ABI headers. |
| `dist/` | Installer script, icons, datatypes, presets. |
| `docs/` | Architecture & ABI documentation, the porting playbook and the original AROS autodocs. |
| `scripts/` | Container build wrapper and the de-AROS porting scripts. |

The AROS baseline this port descends from is recorded in [`AROS-BASELINE`](AROS-BASELINE)
and tagged `aros-extract-base`.

### Documentation

Start at [docs/README.md](docs/README.md) — it indexes the reverse-engineered
architecture documents (the core library and each class driver), the context HCD ABI the
stack speaks to `xhci.device`, the porting playbook, and the implementation plan for the
remaining work. The same directory also carries the original AROS reference manuals
(`poseidon.doc`, `usbclass.doc`, `usbhardware.doc`).
