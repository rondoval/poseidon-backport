# Poseidon — the AROS USB stack, backported to AmigaOS 3.2

Poseidon is the USB stack that brings USB to the Amiga. This repository is a
backport of the modern **AROS** Poseidon stack (now v5.x) to **AmigaOS 3.2**
on m68k.

Poseidon was originally an AmigaOS 68k product by **Chris Hodges** (2002–2009);
the **AROS Development Team** has adopted and maintained it ever since. This
project brings that evolved version back to its original platform, driving a USB
host-controller device such as `xhci.device` (e.g. on PiStorm / Emu68).

## Features

- **Real plug-and-play** USB device handling — hot-plug and unplug at will.
- **Trident** — a Magic User Interface (MUI) preferences / control program for
  configuring the stack, classes, and devices.
- **29 USB class drivers** covering input, storage, networking, serial, audio
  and more (see below).

### Class drivers

| Category | Classes |
|---|---|
| **Hubs** | `hub`, `hubss` (USB 3 SuperSpeed) |
| **Human input (HID)** | `bootmouse`, `bootkeyboard`, `hid` (full HID parser), `egalaxtouch` (touchscreen) |
| **Mass storage & imaging** | `massstorage` (BBB / CBI / UAS), `ptp` (cameras), `dfu` (firmware upgrade), `rawwrap` |
| **Serial & modem** | `cdcacm`, `serialpl2303` (PL2303), `serialcp210x` (CP210x) |
| **Printer** | `printer` (provides `usbparallel.device`) |
| **Networking (SANA-II)** | `cdceth`, `asixeth` (ASIX), `pegasuseth` (Pegasus), `davicometh` (DM9601), `moschipeth` (MosChip), `ethwrap`, `rndis`, `lan78xx` (Microchip LAN78xx) |
| **MIDI & audio** | `simplemidi`, `camdmidi` (CAMD), `audio` (USB audio → `ahi.device`) |
| **Other** | `bluetooth`, `stir4200` (IrDA), `palmpda`, `arosx` (Xbox gamepad) |

## Requirements

- An Amiga running **AmigaOS 3.2**.
- A **68040 / 68060 with FPU** — the binaries are built for `-m68040 -mhard-float`.
- **MUI 5** for the Trident GUI and the per-class configuration dialogs.
- A **USB host-controller device** implementing the Poseidon hardware interface
  (`devices/usbhardware.h`), for example `xhci.device`.

## Installation

Download the release archive, unpack it, and run the included `Install` script
(double-click its icon, or run `Installer Install` from a Shell). It copies
`poseidon.library`, the class drivers, the CLI tools, and Trident into place, and
can optionally add the stack to your startup so USB is available at boot.

## Building from source

The easy path needs only **docker** — `./build.sh` runs the build inside the shared
toolchain container (a public image, pulled automatically; no host toolchain to set up):

```sh
./build.sh --build      # build everything in the container
./build.sh --package    # …and produce the installable build/Poseidon-<ver>.lha
```

`BACKEND=pistorm|serial|off` and `DEBUG=<level>` select the debug build. With no flags
`./build.sh` also uploads to a live Amiga — see [CONTRIBUTING](CONTRIBUTING.md).

### Native build (without the container)

With a local m68k-amigaos toolchain you can drive `cmake` directly. You need **bebbo's
`m68k-amigaos` GCC** at `/opt/m68k-amigaos` with the **NDK 3.2** headers (provides `sfdc`),
the **MUI 5 SDK**, the **NDK 3.2 SANA+Roadshow** package (the ethernet classes'
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

## Tools

CLI commands (installed to `C:`):

- **PsdStackLoader** — brings the stack up (goes in `S:User-Startup`).
- **AddUSBHardware** — attaches a host-controller device (e.g. `xhci.device`).
- **AddUSBClasses** — loads the class drivers.
- **PsdDevLister** — lists connected USB devices (`lsusb`-like).
- **PsdErrorlog** — shows the stack's error log.

Optional per-gadget tools (`DRadioTool`, `PencamTool`, `SonixcamTool`,
`RocketTool`, `PowManTool`, `UPSTool`) ship under `Tools/`.

## Project layout

| Path | Contents |
|---|---|
| `poseidon.library/` | The stack core. |
| `usbclass.library/` | The base meta-class every class inherits (ABI headers). |
| `classes/` | The 29 USB class drivers. |
| `trident/` | The MUI control panel + translations. |
| `c/`, `tools/` | CLI commands and optional gadget tools. |
| `include/` | Public ABI headers. |
| `dist/` | Installer script, icons, datatypes, presets. |
| `docs/` | Original AROS autodocs (`*.doc`). |

## Documentation

The `docs/` directory carries the original AROS reference manuals
(`poseidon.doc`, `usbclass.doc`, `usbhardware.doc`).

## License

Poseidon is distributed under the **AROS Public License (APL) Version 1.1** — see
[LICENSE](LICENSE) for the full text. Chris Hodges placed the original Poseidon
sources into AROS under the APL in 2009.

Some components carry their own licenses (an ISC-licensed driver port, a
BSD-licensed submodule, GPL-licensed icon artwork), and MUI is a runtime
dependency that is not part of this distribution. See [LEGAL](LEGAL) for the full
attribution and third-party notices.

## Credits

- **Chris Hodges** — original author of Poseidon (2002–2009).
- **The AROS Development Team** — maintainers since 2009.
- The third-party authors named in [LEGAL](LEGAL).
