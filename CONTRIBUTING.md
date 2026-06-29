# Contributing to Poseidon (AmigaOS 3.2 backport)

Thanks for your interest! This repository is a backport of the **AROS** Poseidon
USB stack to **AmigaOS 3.2** on m68k. Bug reports, fixes, new class drivers, and
documentation are all welcome.

## Licensing of contributions

By submitting a contribution you agree that it is licensed under the **AROS
Public License (APL) Version 1.1** (see [LICENSE](LICENSE)), consistent with the
rest of the stack. Do not add code under terms incompatible with the APL. Any
third-party code you bring in must carry its own license header and be recorded
in [LEGAL](LEGAL).

## Reporting bugs and requesting features

Please use the issue templates. For bugs, the hardware and version details the
template asks for matter a lot — USB problems are very dependent on the exact
machine, host-controller device, and the device/class involved. A debug log
(see *Debugging* below) is the single most useful thing you can attach.

## Building and testing

The quickest path needs only **docker**: `./build.sh` builds in the shared toolchain
container — no host toolchain to set up. The actions combine freely:

- `./build.sh --build` — build everything in the container
- `./build.sh --package` — build + the installable `.lha`
- `./build.sh --upload` — push the built binaries to a live Amiga over Amiga Explorer
- no flags → `--build --upload`, the fastest edit-build-test loop on real hardware

See the README's [Building from source](README.md#building-from-source) for the
native-toolchain (`cmake`) alternative.

Build the component(s) you touched cleanly (no new warnings) before submitting.

## Debugging

Debug output is **compile-time**, via a switchable backend shared by every
component (`include/debug.h`):

```sh
BACKEND=serial ./build.sh --build      # backend: pistorm (default) | serial | off
#   (native cmake: -DPOSEIDON_DEBUG_BACKEND=serial)
```

Don't add unconditional serial/`kprintf` debug to source — route it through the
backend so a release build stays silent.

## Tracking AROS upstream

The stack preserves AROS history and blame, but its commit SHAs are rewritten, so
you **cannot** `git merge` / `git pull` from AROS. To port an upstream fix,
generate a patch from a tracking AROS clone and apply it into the corresponding
path here:

```sh
git format-patch -1 <aros-sha> -- rom/usb/<path>
git am -p<n> --directory=<our/path>
```

The `AROS-BASELINE` file records the upstream SHA this tree currently tracks from.
Against files that are still close to upstream this applies cleanly; heavily
modified files become manual ports.

## Pull requests

- Branch off `main` and keep each PR focused on one change.
- Note **what** you changed and **how you tested it** — which Amiga / accelerator,
  AmigaOS version, host-controller device, and USB device.
