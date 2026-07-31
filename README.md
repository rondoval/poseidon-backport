# Poseidon for AmigaOS

The USB stack for **AmigaOS 3.2**. Plug in a keyboard, a mouse, a memory stick, a USB
sound card or a network adapter and use it from Workbench — hot-plug, unplug, no reboot.

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

- **AmigaOS 3.2.**
- A **68040 or 68060 with FPU.** The released binaries are built for
  `-m68040 -mhard-float` and will not run on a 68000/010/020/030.
- A **USB host-controller driver** — see [below](#talking-to-your-usb-hardware). Both
  `xhci.device` **6.x** and `xhci.device` **5.x** from the
  [Emu68 driver stack](https://github.com/rondoval/emu68-driver-stack) (PiStorm / Emu68
  on a Raspberry Pi 4 or CM4) are confirmed working.
- **MUI 5**, for Trident and the per-class settings dialogs. The stack itself runs
  without it.

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
[Releases](https://github.com/rondoval/poseidon-backport/releases) page.

| Archive | When to use it |
|---|---|
| `Poseidon-<ver>.lha` | **Start here.** The normal release build. |
| `Poseidon-<ver>-serial.lha` | Identical, but also prints diagnostics to the serial port. Troubleshooting only. |

## Installing

1. Unpack the archive on your Amiga.
2. Run the `Install` script (double-click its icon, or `Installer Install` from a Shell).
3. Answer the questions, then reboot.

The installer copies `poseidon.library` to `LIBS:`, the class drivers to
`SYS:Classes/USB/`, the shell commands to `C:`, and Trident with its icon, translations
and the USB attach/detach sounds to `SYS:Prefs/`. It version-checks everything and never
replaces a newer file without asking. The optional per-gadget tools are a separate
question, and so is the Poseidon preset datatype.

It also offers to add the three startup commands to `S:User-Startup`, which is what you
want unless you prefer to start the stack yourself:

```
PsdStackLoader
AddUSBHardware xhci.device 0     ; your host-controller device and unit
AddUSBClasses
```

If you already run a Poseidon, this upgrades it. Your settings are kept — they live in
`ENVARC:Sys/poseidon.prefs` as they always did, in the classic Poseidon file format.

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
  for FAT, NTFS and CD/DVD partitions; what to automount (RDB, MBR/GPT, CD/DVD) and
  whether to unmount on removal; whether to prefer UAS over the older BOT transport, and
  the UAS queue depth.
- **The device list** — suspend, resume or power-cycle a port by hand. Power-cycling
  often revives a device that has stopped responding.

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

Optional per-gadget tools (`DRadioTool`, `PencamTool`, `SonixcamTool`, `RocketTool`,
`PowManTool`, `UPSTool`) install to `SYS:Tools/`.

## Version numbers

`poseidon.library` keeps its name — every USB class and application opens it by that
name, and that compatibility *is* the point — so the version number is what tells this
line apart from the ones before it. Chris Hodges' classic AmigaOS Poseidon is the **4.x**
line and the AROS one is **5.x**; **Poseidon for AmigaOS is 6.x**, and does not track
AROS's numbering.

Every shipped component carries the same version — **6.0** in this release — and
identifies itself as `Poseidon for AmigaOS` in its `$VER` string. Because the 6.x jump
table extends the classic one, the classes and tools require `poseidon.library` **6** or
newer. Host-controller drivers are a separate matter: they are negotiated by capability,
never by version number, so a driver is never gated on a marketing number.

[`RELEASE-NOTES.md`](RELEASE-NOTES.md) lists what changed in each version, and what is
known not to work yet.

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
./build.sh --build      # build everything in the container
./build.sh --package    # …and produce the installable build/Poseidon-<ver>.lha
```

`BACKEND=pistorm|serial|off` and `DEBUG=<level>` select the debug build. With no flags
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
cmake --build build --target package            # …or build/Poseidon-<ver>.lha
```

**Clean rebuild:** `rm -rf build`, then re-run the configure command (re-passing
`-DCMAKE_TOOLCHAIN_FILE` once the cache is gone).

Targeting a different CPU is a configure option: `-DM68K_CPU=68020 -DM68K_FPU=soft`.

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
