# dist/icons — Workbench icon assets

Source art and generators for the `.info` icons (and the PSD datatype) shipped by
`make package`. The produced `.info` / descriptor files are committed as **static
binary assets** (under `dist/`), so building the stack needs no Python/pypng/
icontool — the files here only matter when *regenerating* them.

## What gets generated

| Output (committed) | From | Kind |
|---|---|---|
| `dist/Install.info` | `installer.png` + `installer.info.src` | project, DefaultTool=`Installer`, ColorIcon |
| `dist/Trident.info` | `Trident.png` + `Trident.info.src` | tool (Stack 57344), ColorIcon |
| `dist/def_PSD.info` | `def_PSD.png` + `def_PSD.info.src` | project deficon, ColorIcon |
| `dist/datatypes/PSD` | `../datatypes/PSD.dtd` | binary DataType descriptor |

All three `.info` files are built by the **same** generator (`make_icons.py`):
each gets a faithful OS3.5 ColorIcon plus a classic planar fallback, with
TYPE/STACK/DEFAULTTOOL taken from its `.info.src`.

## Files here

- `installer.png` + `installer.info.src` — installer icon art (a downward
  Poseidon trident) and its descriptor (project, DefaultTool=`Installer`).
- `Trident.png` + `Trident.info.src` — Trident program icon (AROS Gorilla USB-plug).
- `def_PSD.png` + `def_PSD.info.src` — Poseidon preset-file deficon art (AROS Poseidon tree).
- `amiga_icon_template.py` — shared helper: writes a minimal, self-authored classic
  DiskObject `.info` to import onto (no third-party icon bytes embedded).
- `make_icons.py` — builds all three `.info` files from their PNG + `.info.src`.

The PSD datatype source `../datatypes/PSD.dtd` lives next to its generated binary.

## Requirements (host-side only)

- python3 with **pypng** (icontool reads the PNGs): `pip install pypng` (e.g. in a venv).
- **icontool** with `--import-coloricon` / `--set-defaulttool` — from
  [rondoval/icontool](https://github.com/rondoval/icontool), branch `set-defaulttool`
  (adds the ColorIcon writer + DefaultTool set/clear). Path via `$ICONTOOL`, else `../../../icontool/icontool`.

## Regenerating

```sh
ICONTOOL=/path/to/icontool/icontool python3 make_icons.py   # Install.info, Trident.info, def_PSD.info
```

The PSD datatype descriptor is built from `../datatypes/PSD.dtd` with AROS's
host tool `tools/dtdesc/createdtdesc` (it emits a correct big-endian Amiga
descriptor even on a little-endian host):

```sh
createdtdesc -o ../datatypes/PSD ../datatypes/PSD.dtd
```

## Licensing

`Trident.png` is from the AROS **Gorilla** icon set — **GPL** (see top-level `LEGAL`).
`def_PSD.png` and `PSD.dtd` are from the Poseidon sources (AROS Public License). The
installer art (`installer.png`) is original to this project.
