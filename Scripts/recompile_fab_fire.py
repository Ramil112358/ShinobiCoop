"""Recompiles and saves the Fab fire systems the fire patch uses (/Game/M5VFXVOL2/Niagara/Fire NFire_01..03). Their user-variable
links to the Color modules' Scale Color were made with the Niagara toolsets, which leave a system stale (saved before its compile
finished); run this after editing them. Close the GUI editor. Run with Scripts/run_python.ps1.
"""
import unreal

for name in ("NFire_01", "NFire_02", "NFire_03"):
    path = f"/Game/M5VFXVOL2/Niagara/Fire/{name}"
    system = unreal.load_asset(path)
    ok = unreal.ShinobiNiagaraEditorLibrary.recompile_and_wait(system)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(system)
    unreal.log(f"recompiled {path}: compiled={ok} saved={saved}")
