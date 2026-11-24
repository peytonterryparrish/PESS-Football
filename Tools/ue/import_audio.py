"""Imports ArtWork/audio/*.wav into /Game/PF/Audio (loops for crowd and music). Run with Tools/ue/run_py.sh."""
import os, unreal
ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(ROOT, "ArtWork", "audio")
tasks = []
for f in sorted(os.listdir(SRC)):
    if not f.endswith(".wav"): continue
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, f)
    t.destination_path = "/Game/PF/Audio"
    t.automated = True; t.replace_existing = True; t.save = True
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for f in os.listdir(SRC):
    n = f[:-4]
    a = unreal.load_asset("/Game/PF/Audio/" + n)
    if not a: unreal.log_warning("[PF] missing " + n); continue
    if n.startswith("music") or n.endswith("_loop"):
        a.set_editor_property("looping", True)
        unreal.EditorAssetLibrary.save_asset("/Game/PF/Audio/" + n)
unreal.log("[PF] audio imported %d" % len(tasks))
