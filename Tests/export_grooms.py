"""
Headless smoke test for the groom Alembic exporter.

Run with:
  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript
      -script="<this file>" -unattended -nosplash -nullrhi

Exports every groom under Content/MetaHumans/NewMetaHumanCharacter/Grooms/ and prints a
summary line per asset. Exit status is non-zero if any export fails.
"""

import os
import sys

import unreal

GROOM_PACKAGE_DIR = "/Game/MetaHumans/NewMetaHumanCharacter/Grooms"
OUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "GroomExportTest")

# Ogawa-backed Alembic archives start with this magic.
OGAWA_MAGIC = b"Ogawa"


def find_grooms():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = registry.get_assets_by_path(GROOM_PACKAGE_DIR, recursive=True)
    grooms = []
    for data in assets:
        asset = data.get_asset()
        if isinstance(asset, unreal.GroomAsset):
            grooms.append(asset)
    return sorted(grooms, key=lambda a: a.get_name())


def main():
    out_dir = os.path.abspath(OUT_DIR)
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)

    grooms = find_grooms()
    if not grooms:
        unreal.log_error("No groom assets found under %s" % GROOM_PACKAGE_DIR)
        return 1

    failures = 0
    for groom in grooms:
        name = groom.get_name()
        path = os.path.join(out_dir, name + ".abc")

        raw = unreal.GroomExportBlueprintLibrary.export_groom_to_alembic(groom, path)
        if not isinstance(raw, tuple):
            raw = (raw,)

        # UE decides for itself how a bool return plus out params is packed, so pick the
        # values out by type rather than relying on a fixed arity.
        if groom is grooms[0]:
            unreal.log("raw return: %r" % (raw,))

        ok = next((v for v in raw if isinstance(v, bool)), True)
        error = next((v for v in raw if isinstance(v, str)), "")
        counts = [v for v in raw if isinstance(v, int) and not isinstance(v, bool)]
        counts += [0] * (3 - len(counts))
        groups, curves, points = counts[:3]

        # A non-empty error string means the export failed even if no bool came back.
        if error:
            ok = False

        if not ok:
            failures += 1
            unreal.log_error("FAIL %-24s %s" % (name, error))
            continue

        # The writer reported success, so the file must exist and be a real Ogawa archive.
        if not os.path.isfile(path):
            failures += 1
            unreal.log_error("FAIL %-24s reported success but no file at %s" % (name, path))
            continue

        size = os.path.getsize(path)
        with open(path, "rb") as handle:
            magic = handle.read(5)

        if magic != OGAWA_MAGIC:
            failures += 1
            unreal.log_error(
                "FAIL %-24s not an Ogawa archive (first bytes %r)" % (name, magic)
            )
            continue

        unreal.log(
            "OK   %-24s groups=%-2d curves=%-7d points=%-9d %8.1f KB"
            % (name, groups, curves, points, size / 1024.0)
        )

    unreal.log("---- %d groom(s), %d failure(s) ----" % (len(grooms), failures))
    return 1 if failures else 0


sys.exit(main())
