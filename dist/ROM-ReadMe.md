# USB in ROM — building a custom Kickstart

**This guide is for PiStorm/Emu68 machines.** It builds a custom 2 MB Kickstart image with the USB
stack inside it, which gets you:

* a **USB mouse and keyboard in the boot menu**
* **booting from a USB drive** (or from NVMe)

Normally the stack is started from `S:User-Startup`, far too late for either.

Poseidon can go into ROM on other Amigas too, but you will have to adapt the steps below to your
own setup.

It is optional, and it is reversible. Nothing on your Amiga's hard drive changes — the normal
installation stays exactly as it is and keeps working. Everything happens in one file on the SD
card and one line of `config.txt`, so undoing it is a one-line edit.

You do all of this on a **PC** — Linux, or Windows with WSL — not on the Amiga.

## What you need

* **Python and amitools:** `pipx install amitools` (or `pip install amitools`).
* **This archive, unpacked.** The script takes `poseidon.library` and the USB classes from the
  `Libs/` and `Classes/USB/` drawers next to this one, so leave it where you unpacked it.
* **Your own Kickstart** — the plain 512 KiB AmigaOS 3.2 ROM file Emu68 already loads, copied off
  the SD card.
* **The Emu68 driver archive**, unpacked —
  [emu68-driver-stack](https://github.com/rondoval/emu68-driver-stack). Two files from it are
  **required**, or USB will not come up at boot: `LIBS/bcmpcie.library` and
  `DEVS/USBHardware/xhci.device`. Add `DEVS/nvme.device` if you want to boot from NVMe.
* **Only if you want to boot from CD:** `ODFileSystem`.

## Build it

One command. The image is written to the directory you run it from, as `kick-usb-2m.rom`:

```
bash ROM/build-kickstart.sh  kick.rom \
     ../emu68-drivers/LIBS/bcmpcie.library \
     ../emu68-drivers/DEVS/USBHardware/xhci.device
```

Add more files to the end of the line to put more in the ROM — `nvme.device`, `ODFileSystem`.
The Poseidon parts are found on their own; you never list those.

The script prints what went into the image, with version numbers:

```
   -41   NT_LIBRARY   bcmpcie.library    $VER: bcmpcie.library 2.3 ...
   -42   NT_DEVICE    xhci.device        $VER: emu68-xhci-driver 6.1 ...
   -44   NT_LIBRARY   poseidon.library   ...
```

**Read that list before you use the image.** It is the only place the versions inside the ROM are
visible, and the one check that you embedded the drivers you meant to.

## Put it on the SD card

Copy `kick-usb-2m.rom` onto the SD card's FAT partition, next to the Kickstart file that is there
now, and point the `initramfs` line in `config.txt` at it instead.

`S:User-Startup` needs no changes at all. `PsdStackLoader` still loads your saved settings and
hands the keyboard and mouse over to the full `hid.class`, and `AddUSBClasses` still adds the
twenty-odd classes that are not in the ROM. The `AddUSBHardware` line no longer does anything —
the ROM has already added the controller — but it does no harm.

## Good to know

**The ROM copies win.** Once `xhci.device`, `nvme.device` and `bcmpcie.library` are in the ROM, the
copies in `DEVS:` and `LIBS:` are never used. So when you update the driver archive, build the ROM
again — otherwise your machine keeps running the old drivers.

## If something goes wrong

**The machine does not boot, or hangs before the boot menu.** Put the old `initramfs` line back.
This is why you kept the original Kickstart.

**It boots, but USB does not work.** Check the list the script printed: `bcmpcie.library` and
`xhci.device` both have to be in it. If they are, look at the log in Trident (Poseidon's
preferences program) — `No xhci.device unit found` there means the driver did not start, and the
usual cause is a `-rangeops` driver on firmware without the cache extensions.

**A USB drive works, but is not offered in the boot menu.** Is it RDB and bootable?
For a CD, check that you passed `ODFileSystem`.

**`romtool not found`.** amitools is not installed, or `pipx`'s directory is not on your PATH.
