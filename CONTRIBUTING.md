# Contributing to Poseidon for AmigaOS

Thanks for your interest! This repository is **Poseidon for AmigaOS** — the 6.x
line of the Poseidon USB stack, backported from the AROS 5.x line to **AmigaOS
3.2** on m68k. Bug reports, fixes, new class drivers, and documentation are all
welcome.

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

The stack preserves AROS history and blame, but its commit SHAs were rewritten at
extraction and its paths differ (`rom/usb/classes/…` is `classes/…` here), so you
**cannot** `git merge`, `git pull` or `git cherry-pick` from AROS. Upstream fixes are
ported one commit at a time, and each port keeps its upstream author.

Two files in the repository root say where we stand:

- **`AROS-BASELINE`** — the AROS commit this tree was extracted from (tagged
  `aros-extract-base`). It never changes.
- **`AROS-SYNC`** — the AROS commit up to which upstream `rom/usb` has been reviewed.
  Every upstream commit at or before it has been considered: it was ported, or judged
  superseded by our own code or not applicable here. It does *not* mean our tree equals
  upstream at that commit. It is written when a sync cycle closes; until the first one
  has, the range starts at `AROS-BASELINE`.

What was ported is recorded in two places: each port's commit carries an `AROS-commit:`
trailer (below), and the release notes list the ported fixes a user would notice.

### Porting a commit

You need a clone of [AROS](https://github.com/aros-development-team/AROS) that holds
the commit and its parent; a blobless one is enough
(`git clone --filter=blob:none --no-checkout`).

```sh
scripts/aros-port.sh <aros-clone> <sha> [<upstream-path> ...]
```

The script applies the commit's changes to our files with a three-way merge and leaves
the result **staged** — it never commits. It maps the paths (`rom/usb/classes` →
`classes`, `rom/usb/poseidon` → `poseidon.library`, `rom/usb/poseidon/shellcommands` →
`c`, `rom/usb/shellapps` → `tools`, `rom/usb/trident` → `trident`), skips AROS build
files, and reports anything it could not place: files with conflicts (ordinary conflict
markers — resolve and `git add`), files that were renamed or removed here, and files we
have no place for, such as the host-controller drivers. It needs a clean index, so a
port is never mixed with other work.

It then writes the proposed commit message to `AROS_PORT_MSG` in the repository root (a
git-ignored scratch file — edit it freely) and prints the commit line, which carries the
upstream author and date:

```sh
git commit --author='Upstream Author <address>' --date='<upstream date>' -F AROS_PORT_MSG
```

"Applied cleanly" is not "builds". Upstream code still uses AROS idioms this tree has
dropped (`GM_UNIQUENAME()`, `AROS_LH…`, bare message strings where we use `psdTxt()`),
so build before committing — [docs/porting-playbook.md](docs/porting-playbook.md) is the
recipe for those.

### How ports are committed

Every port ends in the trailer `AROS-commit: <full sha>`, so
`git log --grep='^AROS-commit:'` lists exactly which upstream commits are in the tree.
The script adds it.

| Shape | When | Author | Message |
|---|---|---|---|
| **Port** | The upstream change is what we want, as written or with fix-ups that only make it build | Upstream author and date | Upstream subject and body unchanged; a `[yourname: …]` line for each fix-up; the trailer |
| **Follow-up** | We want more than upstream did — another call site, a narrower condition | You | Your own; refers to the port it builds on |
| **Adapted** | The diagnosis is upstream's but the patch does not fit and the code is ours | You | Your own, saying what was taken; the trailer; `Co-authored-by:` the upstream author when lines of theirs survive |

- **Port first, refine second.** If upstream's patch applies but should be changed, land
  it as a Port and put the change in a Follow-up, so `git blame` shows who wrote which
  line.
- **Fix-ups that only make it build go in the Port commit**, named in a bracketed line in
  its own paragraph above the trailer — the Linux-kernel convention for "whoever carried
  this patch changed it". (Keep the blank line: a non-trailer line inside the last
  paragraph stops git from recognising the trailer.)
- **The subject may gain a component prefix**, nothing else: upstream's `minor refactor.`
  becomes `ptp.class: minor refactor.`
- **Partial takes say so.** When only part of an upstream commit is taken, or one
  upstream commit lands here as several, run the script with `--partial`; the trailer
  then reads `AROS-commit: <sha> (partial)`.

### A sync cycle

1. Update the AROS clone and list what is new:
   `git log --reverse --no-merges $(cat AROS-SYNC)..origin/master -- rom/usb`.
   If the clone is shallow, make sure it reaches well behind `AROS-SYNC` — a tight
   `--shallow-since` can cut the history at a later merge and silently hide commits.
2. Review every commit against our tree and decide for each: port, adapted, superseded,
   not applicable, or deferred. The review is a working document for the cycle; it is
   not kept.
3. Port what was accepted. A port that changes something a user would notice adds its
   line to the release notes in the same commit.
4. Close the cycle by moving `AROS-SYNC` to the head that was reviewed; anything deferred
   goes to [docs/implementation-plan.md](docs/implementation-plan.md).

Host-controller commits (`rom/usb/pcixhci`, `pciusb`) never port — those drivers are not
in this tree — but the bugs they fix are worth checking against `xhci.device`.

## Pull requests

- Branch off `main` and keep each PR focused on one change.
- Note **what** you changed and **how you tested it** — which Amiga / accelerator,
  AmigaOS version, host-controller device, and USB device.
