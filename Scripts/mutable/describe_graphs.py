"""Writes how each Customizable Object under a folder is wired (nodes, edited properties, links), one file per object.

  SHINOBI_DESCRIBE_DIR=/Game/Shinobi/Mutable/_Reference SHINOBI_DESCRIBE_OUT=<folder> Scripts/run_python.ps1 -Script Scripts/mutable/describe_graphs.py
"""
import os

import unreal

FOLDER = os.environ.get("SHINOBI_DESCRIBE_DIR", "/Game/Shinobi/Mutable")
OUT = os.environ.get("SHINOBI_DESCRIBE_OUT", os.path.join(unreal.Paths.project_saved_dir(), "MutableGraphs"))
os.makedirs(OUT, exist_ok=True)

for path in unreal.EditorAssetLibrary.list_assets(FOLDER, recursive=True):
    asset = unreal.load_asset(path.split(".")[0])
    if isinstance(asset, unreal.CustomizableObject):
        text = unreal.ShinobiMutableGraphLibrary.describe_graph(asset)
        with open(os.path.join(OUT, asset.get_name() + ".txt"), "w", encoding="utf-8") as f:
            f.write(text)
        unreal.log(f"[describe] {asset.get_name()}: {text.count(chr(10))} lines")
