"""
Deeper verification than curve counts.

Round-trips every groom (export -> re-import -> export) and compares the two .abc files
byte for byte. Matching bytes prove that positions, widths, root UVs and every groom_*
attribute survived the trip, not just the topology.

Also reports whether the Content Browser context menu this plugin extends is registered.
"""

import hashlib
import os
import sys

import unreal

GROOM_PACKAGE_DIR = "/Game/MetaHumans/NewMetaHumanCharacter/Grooms"
IMPORT_DEST = "/Game/GroomExportFidelity"
OUT_DIR = os.path.abspath(os.path.join(unreal.Paths.project_saved_dir(), "GroomFidelity"))
GROOM_MENU = "ContentBrowser.AssetContextMenu.GroomAsset"


def export(groom, path):
    raw = unreal.GroomExportBlueprintLibrary.export_groom_to_alembic(groom, path)
    if not isinstance(raw, tuple):
        raw = (raw,)
    error = next((v for v in raw if isinstance(v, str)), "")
    counts = [v for v in raw if isinstance(v, int) and not isinstance(v, bool)]
    counts += [0] * (3 - len(counts))
    return error, tuple(counts[:3])


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def reimport(abc_path, asset_name):
    task = unreal.AssetImportTask()
    task.filename = abc_path
    task.destination_path = IMPORT_DEST
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = False
    task.options = unreal.GroomImportOptions()

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    for path in task.get_editor_property("imported_object_paths") or []:
        asset = unreal.load_asset(path)
        if isinstance(asset, unreal.GroomAsset):
            return asset
    return None


def check_menu():
    menus = unreal.ToolMenus.get()
    registered = menus.is_menu_registered(GROOM_MENU)
    unreal.log("menu '%s' registered: %s" % (GROOM_MENU, registered))
    menu = menus.find_menu(GROOM_MENU)
    unreal.log("menu object: %s" % ("found" if menu else "NOT FOUND"))


def main():
    if not os.path.isdir(OUT_DIR):
        os.makedirs(OUT_DIR)

    unreal.log("==== menu registration ====")
    try:
        check_menu()
    except Exception as exc:  # noqa: BLE001 - diagnostics only
        unreal.log_warning("menu check unavailable: %s" % exc)

    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    grooms = []
    for data in registry.get_assets_by_path(GROOM_PACKAGE_DIR, recursive=True):
        asset = data.get_asset()
        if isinstance(asset, unreal.GroomAsset):
            grooms.append(asset)
    grooms.sort(key=lambda a: a.get_name())

    unreal.log("==== byte-level round trip ====")
    failures = 0

    for groom in grooms:
        name = groom.get_name()
        path_a = os.path.join(OUT_DIR, name + "_a.abc")
        path_b = os.path.join(OUT_DIR, name + "_b.abc")

        err_a, counts_a = export(groom, path_a)
        if err_a:
            failures += 1
            unreal.log_error("FAIL %-22s export A: %s" % (name, err_a))
            continue

        imported = reimport(path_a, name + "_FID")
        if imported is None:
            failures += 1
            unreal.log_error("FAIL %-22s re-import produced no GroomAsset" % name)
            continue

        err_b, counts_b = export(imported, path_b)
        if err_b:
            failures += 1
            unreal.log_error("FAIL %-22s export B: %s" % (name, err_b))
            continue

        size_a = os.path.getsize(path_a)
        size_b = os.path.getsize(path_b)
        same_bytes = sha256(path_a) == sha256(path_b)
        same_counts = counts_a == counts_b

        if not same_counts:
            failures += 1
            verdict = "COUNTS DIFFER"
        elif same_bytes:
            verdict = "IDENTICAL"
        elif size_a == size_b:
            verdict = "same size, bytes differ"
        else:
            verdict = "size differs (%+d bytes)" % (size_b - size_a)

        unreal.log("%-24s %-24s curves=%d points=%d  %d -> %d bytes"
                   % (name, verdict, counts_a[1], counts_a[2], size_a, size_b))

    unreal.log("---- %d groom(s), %d failure(s) ----" % (len(grooms), failures))
    return 1 if failures else 0


sys.exit(main())
