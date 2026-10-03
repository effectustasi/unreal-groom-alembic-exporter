"""Exports Eyebrows_M_Full twice - Unreal Z-up and Y-up - for external DCC checking."""

import os
import sys

import unreal

ASSET = "/Game/MetaHumans/NewMetaHumanCharacter/Grooms/Eyebrows_M_Full"
OUT_DIR = os.path.abspath(os.path.join(unreal.Paths.project_saved_dir(), "GroomDCCCheck"))


def export(groom, path, y_up):
    raw = unreal.GroomExportBlueprintLibrary.export_groom_to_alembic(groom, path, y_up)
    if not isinstance(raw, tuple):
        raw = (raw,)
    error = next((v for v in raw if isinstance(v, str)), "")
    counts = [v for v in raw if isinstance(v, int) and not isinstance(v, bool)]
    counts += [0] * (3 - len(counts))
    return error, counts


def main():
    if not os.path.isdir(OUT_DIR):
        os.makedirs(OUT_DIR)

    groom = unreal.load_asset(ASSET)
    if groom is None:
        unreal.log_error("could not load %s" % ASSET)
        return 1

    for y_up, tag in ((False, "zup"), (True, "yup")):
        path = os.path.join(OUT_DIR, "Eyebrows_%s.abc" % tag)
        err, counts = export(groom, path, y_up)
        if err:
            unreal.log_error("FAIL %s: %s" % (tag, err))
            return 1
        unreal.log("OK   %-4s %s groups=%d curves=%d points=%d"
                   % (tag, path, counts[0], counts[1], counts[2]))

    return 0


sys.exit(main())
