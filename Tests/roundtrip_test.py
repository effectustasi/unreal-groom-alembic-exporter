"""
Round-trip verification for the groom Alembic exporter.

For each groom it:
  1. exports to .abc in Unreal space (the round-trip safe mode),
  2. re-imports that .abc with the groom importer's default conversion settings,
  3. exports the re-imported groom again,
  4. compares curve/point counts between step 1 and step 3.

Identical counts mean the file carried the groom's full topology back into Unreal.

Also dumps every asset in the Grooms folder with its class, to show which ones are
actually GroomAssets.
"""

import os
import sys

import unreal

GROOM_PACKAGE_DIR = "/Game/MetaHumans/NewMetaHumanCharacter/Grooms"
IMPORT_DEST = "/Game/GroomExportRoundTrip"
OUT_DIR = os.path.abspath(os.path.join(unreal.Paths.project_saved_dir(), "GroomRoundTrip"))


def export(groom, path):
    """Returns (error, groups, curves, points). UE packs only the out params."""
    raw = unreal.GroomExportBlueprintLibrary.export_groom_to_alembic(groom, path)
    if not isinstance(raw, tuple):
        raw = (raw,)
    error = next((v for v in raw if isinstance(v, str)), "")
    counts = [v for v in raw if isinstance(v, int) and not isinstance(v, bool)]
    counts += [0] * (3 - len(counts))
    return error, counts[0], counts[1], counts[2]


def dump_folder_contents():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    unreal.log("==== assets under %s ====" % GROOM_PACKAGE_DIR)
    for data in registry.get_assets_by_path(GROOM_PACKAGE_DIR, recursive=True):
        unreal.log(
            "  %-42s %s" % (str(data.asset_name), str(data.asset_class_path.asset_name))
        )


def reimport(abc_path, asset_name):
    task = unreal.AssetImportTask()
    task.filename = abc_path
    task.destination_path = IMPORT_DEST
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = False

    options = unreal.GroomImportOptions()
    # Leave conversion settings at their defaults (rotation 0,0,0 / scale 1,1,1); that is
    # what makes an Unreal-space export round-trip exactly.
    task.options = options

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    for path in task.get_editor_property("imported_object_paths") or []:
        asset = unreal.load_asset(path)
        if isinstance(asset, unreal.GroomAsset):
            return asset
    return None


def main():
    if not os.path.isdir(OUT_DIR):
        os.makedirs(OUT_DIR)

    dump_folder_contents()

    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    grooms = []
    for data in registry.get_assets_by_path(GROOM_PACKAGE_DIR, recursive=True):
        asset = data.get_asset()
        if isinstance(asset, unreal.GroomAsset):
            grooms.append(asset)
    grooms.sort(key=lambda a: a.get_name())

    unreal.log("==== round trip ====")
    failures = 0

    for groom in grooms:
        name = groom.get_name()
        first_path = os.path.join(OUT_DIR, name + ".abc")

        err, g1, c1, p1 = export(groom, first_path)
        if err:
            failures += 1
            unreal.log_error("FAIL %-24s export 1: %s" % (name, err))
            continue

        imported = reimport(first_path, name + "_RT")
        if imported is None:
            failures += 1
            unreal.log_error("FAIL %-24s re-import produced no GroomAsset" % name)
            continue

        second_path = os.path.join(OUT_DIR, name + "_RT.abc")
        err2, g2, c2, p2 = export(imported, second_path)
        if err2:
            failures += 1
            unreal.log_error("FAIL %-24s export 2: %s" % (name, err2))
            continue

        match = (g1, c1, p1) == (g2, c2, p2)
        if not match:
            failures += 1

        unreal.log(
            "%s %-24s orig groups=%d curves=%d points=%d | rt groups=%d curves=%d points=%d"
            % ("OK  " if match else "DIFF", name, g1, c1, p1, g2, c2, p2)
        )

    unreal.log("---- %d groom(s), %d failure(s) ----" % (len(grooms), failures))
    return 1 if failures else 0


sys.exit(main())
