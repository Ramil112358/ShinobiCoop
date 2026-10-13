"""Copies the Game Animation Sample's CMC motion-matching AnimBP and everything it references into ShinobiCoop.

Run headless against the GASP project (created from Fab at E:\\GASP), with both editors closed:

  UnrealEditor-Cmd.exe E:\\GASP\\GameAnimationSample.uproject -run=pythonscript -script=E:\\ShinobiCoop\\Scripts\\anim\\migrate_gasp.py -unattended -nop4 -nosplash

Packages keep their /Game paths (/Game/Blueprints, /Game/Characters/UEFN_Mannequin, /Game/Audio/Foley, ...); a dry run
on 2026-10-12 found no clashes with existing ShinobiCoop assets. Roots are the AnimBP only: the GASP character
Blueprints drag in Echo, Paragon and MetaHuman content (4.7 GB) through their visual override manager, which we
don't need. ShinobiCoop.uproject must already enable the plugins this content needs (PoseSearch, BlendStack, Chooser,
MotionWarping, AnimationWarping, CurveExpression, DrawDebugLibrary, Mover). Not MovieSceneAnimMixer, which GASP enables:
with it on, every Slot node calls into the Sequencer mixer and asserts in any AnimBP compiled without its subsystem.

Idempotent: existing destination files are left alone, so re-running only adds what's missing.
"""
import os
import unreal

DEST_CONTENT = "E:/ShinobiCoop/Content"
ROOTS = [
    "/Game/Blueprints/SandboxCharacter_CMC_ABP",
    # Traversal montages and their chooser (UShinobiTraversalComponent). Not AC_TraversalLogic, the sample's traversal
    # component: it only works on the sample's level blocks and drags them in.
    "/Game/Characters/UEFN_Mannequin/Animations/Traversal/CHT_TraversalMontages_CMC",
]


def closure(roots):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True,
                                                    include_searchable_names=False, include_soft_management_references=False,
                                                    include_hard_management_references=False)
    seen, stack = set(), list(roots)
    while stack:
        package = stack.pop()
        if package in seen:
            continue
        seen.add(package)
        for dependency in registry.get_dependencies(package, options) or []:
            dependency = str(dependency)
            if dependency.startswith("/Game/") and dependency not in seen:
                stack.append(dependency)
    return sorted(seen)


def main():
    packages = closure(ROOTS)
    missing = [p for p in packages
               if not os.path.exists(os.path.join(DEST_CONTENT, p[len("/Game/"):] + ".uasset"))
               and not os.path.exists(os.path.join(DEST_CONTENT, p[len("/Game/"):] + ".umap"))]
    unreal.log(f"migrate_gasp: {len(packages)} packages in closure, {len(missing)} to copy")
    if missing:
        options = unreal.MigrationOptions()
        options.set_editor_property("prompt", False)  # no package report dialog in a commandlet
        unreal.AssetToolsHelpers.get_asset_tools().migrate_packages(missing, DEST_CONTENT, options)
    still_missing = [p for p in missing if not os.path.exists(os.path.join(DEST_CONTENT, p[len("/Game/"):] + ".uasset"))
                     and not os.path.exists(os.path.join(DEST_CONTENT, p[len("/Game/"):] + ".umap"))]
    unreal.log(f"migrate_gasp: done, {len(still_missing)} not copied")
    for p in still_missing[:50]:
        unreal.log_warning(f"migrate_gasp: not copied {p}")


main()
