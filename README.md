# Groom Alembic Exporter (UE 5.7)

Adds **Export Groom to Alembic (.abc)…** to the Content Browser context menu of Groom
Assets. UE ships a groom Alembic *importer* but no exporter; this plugin fills that gap.

## Install

1. Your project must be a **C++ project** (a Blueprint-only project has no build target for code plugins). If it isn't, add any C++ class once via *Tools → New C++ Class*; UE converts the project for you.
2. Copy this repository into your project as `Plugins/GroomExport/`.
3. Open the `.uproject` and accept the "rebuild modules?" prompt, or build from the command line (see [Building](#building)).

Requires the **HairStrands** and **AlembicImporter** plugins (both ship with UE and are enabled automatically as dependencies).

## Status

Builds and works on UE 5.7. Verified on 2026-08-28 against MetaHuman grooms.

| Check | Result |
| --- | --- |
| Compiles, editor DLL links | ✅ |
| Exports valid Ogawa `.abc` | ✅ all 5 groom assets |
| Blender reads it as curves, correct scale | ✅ 1508 curves / 15631 points, centimetres |
| Round trip back into Unreal | ✅ re-exported file is **byte-identical** on all 5 |
| Content Browser menu entry registered | ✅ via `IsExportMenuEntryRegistered()` in a GUI session |

Export numbers:

| Groom | Curves | Points |
| --- | ---: | ---: |
| Beard_S_Full | 16,452 | 180,842 |
| Eyebrows_M_Full | 1,508 | 15,631 |
| Eyelashes_S_Thin | 1,258 | 13,334 |
| GroomAsset_0 | 102,827 | 808,593 |
| Mustache_S_Full | 2,781 | 53,690 |

Note on the asset list: `Hair_S_BrushCut` is a **GroomBindingAsset**, not a groom. The head
hair's strand data lives in `GroomAsset_0` — the 102,827-curve entry above. Those five are
every `GroomAsset` in the Grooms folder.

## What it writes

A single archive per groom, laid out the way UE's own importer reads it
(`Engine/Plugins/Importers/AlembicHairImporter/…/AlembicHairTranslator.cpp`):

```
/groom                 OXform, identity. Carries groom-scope user properties
                       (groom_version_major/minor, groom_tool, plus whatever
                       groom-scope attributes the source asset had).
  /group_0             OCurves — one per hair group
  /group_1             OCurves
  ...
```

Each `OCurves` object carries:

| Channel | Content |
| --- | --- |
| `P` | Point positions |
| `nVertices` | Point count per curve |
| curve type | `kLinear` / `kNonPeriodic` / `kNoBasis` — hair strands are polylines, not splines |
| `width` | Vertex scope when the groom has per-point widths, else uniform scope per strand |
| `uv` | Root UV, uniform scope |
| `arbGeomParams` | Every remaining `groom_*` attribute: `groom_group_id`, `groom_id`, `groom_guide`, `groom_clumpid`, `groom_color`, … |

Strand attributes are written at **uniform** scope and point attributes at **vertex**
scope, which is exactly what the importer's `ConvertAlembicAttribute` expects.

### Where the data comes from

The exporter reads `UGroomAsset::GetHairDescription()` — the editor-only source
description. That is the lossless representation: full curve count, float positions, and
all original attributes.

It deliberately does **not** read `HairGroupsPlatformData[i].Strands.BulkData`. That data
is decimated and quantized for rendering, so exporting it would silently produce a
degraded groom. If a groom has no source description (cooked asset, or editor-only data
stripped), the export fails with an explanatory message instead of writing bad data.

## Coordinate space

The importer applies **no** hardcoded axis conversion. It multiplies incoming positions by
whatever the user set in the import dialog's conversion settings, whose defaults are
Rotation `(0,0,0)` / Scale `(1,1,1)`, and it treats Alembic units as centimeters
(`UNIT_TO_CM = 1`).

So the export offers two modes:

- **Unreal Z-up (default).** Positions written exactly as stored. Re-importing with the
  importer's default conversion settings reproduces the original groom — this is the
  round-trip safe option.
- **Y-up (Maya / Blender).** Swaps Y and Z: `(X, Y, Z)_unreal → (X, Z, Y)_alembic`. The
  swap has determinant −1, which is what a left-handed → right-handed basis change
  requires, so the groom keeps its shape and is **not** mirrored. To bring such a file back
  into Unreal, set the importer's conversion rotation to `(90, 0, 0)`.

`Unit scale` (default `1.0`) scales positions and widths; use `0.01` to write meters.

## Building

You need a C++ toolchain for UE. The Build Tools package is enough; the full Visual Studio
IDE is not required:

```bash
winget install --id Microsoft.VisualStudio.2022.BuildTools --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.22621 --includeRecommended"
```

Run that from an **Administrator** terminal — the install needs elevation and takes a
while. If you would rather have the full IDE, use
`Microsoft.VisualStudio.2022.Community` with the same `--add` arguments.

UE also needs the **.NET Framework SDK**, which is not in the Build Tools product graph
(`Microsoft.Net.Component.4.6.2.SDK` fails with "Cannot find package in product graph", and
the *targeting pack* is not a substitute). Without it UBT refuses to build `SwarmInterface`.
Install it standalone:

```bash
winget install --id Microsoft.DotNet.Framework.DeveloperPack_4
```

### Toolchain gotchas worth knowing

- **UE's banned MSVC list uses the compiler version, not the folder name.** The folder
  `MSVC\14.44.35207` holds compiler *14.44.35228*; UBT reports the latter and accepts it,
  even though 35207 would fall inside `BannedVisualCppVersions` (`14.44.0-14.44.35210`)
  from `Engine/Config/Windows/Windows_SDK.json`. Don't panic at the folder name.
- **`--wait` is not valid for the installed `setup.exe modify`** (only for the
  `vs_BuildTools.exe` bootstrapper). Passing it fails with exit code 87.

Then build:

```bash
"<UE_ROOT>\Engine\Build\BatchFiles\Build.bat" <YourProject>Editor Win64 Development -Project="<path>\<YourProject>.uproject" -WaitMutex
```

Or just open your `.uproject` and accept the "rebuild modules?" prompt.

## Using it

1. Right-click one or more Groom Assets in the Content Browser.
2. Choose **Export Groom to Alembic (.abc)…**.
3. Pick options, then a destination. A single selection asks for a file name; a
   multi-selection asks for a folder once and writes `<AssetName>.abc` per groom. The
   options dialog offers "use these settings for all selected grooms" so a batch does not
   re-prompt.

Failures are reported per asset in a notification and logged under `LogGroomExport` /
`LogGroomExportEditor`.

## Verifying it

`Tests/` holds the scripts used to verify this, all runnable headlessly. They point at the
default MetaHuman groom folder (`/Game/MetaHumans/NewMetaHumanCharacter/Grooms`); change the
constants at the top of each script to test your own assets.

| Script | What it does |
| --- | --- |
| `export_grooms.py` | Exports every groom in the Grooms folder, checks each file starts with the Ogawa magic, prints curve/point counts |
| `roundtrip_test.py` | Export → re-import → export again, and compares counts. Also dumps every asset in the folder with its class |
| `export_both_modes.py` | Writes `Eyebrows_zup.abc` and `Eyebrows_yup.abc` for external DCC checking |
| `verify_fidelity.py` | Round-trips and compares the two `.abc` files byte for byte — the strongest fidelity check |
| `check_menu.py` | Confirms the context-menu entry registered. **GUI session only** (see below) |

`check_menu.py` cannot run as a commandlet: without a Content Browser the menu is never
built and you get a false negative. Run it in a real editor session, which quits itself:

```bash
"<UE_ROOT>\Engine\Binaries\Win64\UnrealEditor.exe" "<path>\<YourProject>.uproject" -ExecCmds="py C:/path/without/spaces/check_menu.py, QUIT_EDITOR" -unattended -nosplash -nopause -stdout
```

Put the script somewhere without spaces — quotes nested inside `-ExecCmds` do not survive
cmd plus UE's parser, and the command silently never runs.

It asks the plugin's own `IsExportMenuEntryRegistered()` rather than inspecting the menu
from Python, because `UToolMenu::Sections` is protected and Python cannot read it — a
Python-side check reports "missing" no matter what.

Run one with:

```bash
"<UE_ROOT>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<path>\<YourProject>.uproject" -run=pythonscript -script="<path>/<YourProject>/Plugins/GroomExport/Tests/roundtrip_test.py" -unattended -nosplash -nopause -stdout
```

Two things that will bite you when scripting this:

- **Use forward slashes in `-script=`.** UE processes backslash escapes inside the quoted
  value, so `...\Tests\roundtrip_test.py` silently truncates at the `\r` of "roundtrip" and
  you get `Could not load Python file '...\Tests`.
- **The Python return is out-params only.** `export_groom_to_alembic` returns
  `(error, groups, curves, points)` — the C++ `bool` return is not part of the tuple. The
  test scripts pick values out by type rather than assuming an arity.

### Blender check (Eyebrows_M_Full)

Imported into Blender 4.4 as a single `CURVES` object, 1508 curves / 15631 points — the
same counts Unreal reports.

| File | Blender world bbox | Reading |
| --- | --- | --- |
| `Eyebrows_yup.abc` | Z 178.5 – 181.3 | ✅ eyebrows 1.8 m up, i.e. head height. Correct. |
| `Eyebrows_zup.abc` | Z 9.2 – 14.3 | lying on its side — expected, this file is in Unreal space |

Both have the same bbox dimensions, just permuted (13.00 × 2.72 × 5.04 vs
13.00 × 5.04 × 2.72), so there is no scaling error and no mirroring. 13 cm across for a
pair of eyebrows is right.

Blender's Alembic importer assumes the file is Y-up, which is why the Y-up export is the
one that lands correctly there — and why the Unreal-space export, the one that round-trips
perfectly, looks rotated in Blender. Pick the mode for the destination.

## Notes

- Cards meshes are out of scope. The `*_CardsMesh_Group0_LOD*` assets in the Grooms folder
  are ordinary static meshes and export as FBX through the normal asset export path. This
  plugin handles strand data only.
- `Eyelashes_S_Thin` has no cards mesh — strands only.
- Only tested on UE 5.7 / Windows. Reports for other engine versions and platforms are very welcome.

## License

MIT. Not affiliated with or endorsed by Epic Games. Unreal Engine and MetaHuman are trademarks of Epic Games, Inc.
