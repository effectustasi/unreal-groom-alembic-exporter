"""
Confirms the Content Browser context-menu entry registered.

GUI session only - a commandlet never builds the Content Browser menus, so running this
headlessly reports a false negative.

Asks the plugin's own C++ check rather than inspecting the menu from Python:
UToolMenu::Sections is protected, so a Python-side walk always reports "missing".
"""

import os

import unreal

ok = unreal.GroomExportBlueprintLibrary.is_export_menu_entry_registered()
unreal.log("MENUCHECK entry_registered=%s" % ok)

result_path = os.path.join(
    os.path.abspath(unreal.Paths.project_saved_dir()), "menucheck_result.txt"
)
with open(result_path, "w") as fh:
    fh.write("entry_registered=%s\n" % ok)

unreal.log("MENUCHECK wrote %s" % result_path)
