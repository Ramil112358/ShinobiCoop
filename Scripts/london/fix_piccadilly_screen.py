"""Wraps the Piccadilly screen round its building (build_london.screen_wrap) in maps built before place_landmarks did
it, without a rebuild; re-runnable (it searches again from the actor's ScreenFrom:<x>,<y> tag). UE Python commandlet:
  $env:SHINOBI_MAPS = "/Game/Shinobi/Maps/Lvl_Arena_WestEnd,/Game/Shinobi/Maps/Lvl_London"   # the default
  Scripts/run_python.ps1 -TimeoutSec 1800 Scripts/london/fix_piccadilly_screen.py
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_london as bl  # noqa: E402

MAPS = os.environ.get("SHINOBI_MAPS", "/Game/Shinobi/Maps/Lvl_Arena_WestEnd,/Game/Shinobi/Maps/Lvl_London")
PIECE = "/Game/Shinobi/London/Pieces/piccadilly_lights/piccadilly_lights/StaticMeshes/piccadilly_lights"

for map_path in MAPS.split(","):
    world = bl.load_map(map_path)
    screens = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor)
               if a.get_actor_label() == "Landmark_piccadilly_lights"]
    if not screens:
        bl.log(f"{map_path}: no Piccadilly screen")
        continue
    for screen in screens:
        tags = [str(t) for t in screen.tags]
        start = next((tuple(map(float, t.split(":")[1].split(","))) for t in tags if t.startswith("ScreenFrom:")), None)
        if start is None:  # the old free-standing piece: search from 3 m in front of it
            loc, fwd = screen.get_actor_location(), screen.get_actor_forward_vector()
            start = (loc.x + fwd.x * 300, loc.y + fwd.y * 300)
        wrap = bl.screen_wrap(world, start[0], start[1], unreal.load_asset(PIECE))
        if wrap is None:
            raise RuntimeError(f"{map_path}: no building face near the Piccadilly screen (search from {start})")
        mesh, (x, y, z) = wrap
        screen.modify()  # else the commandlet never marks its external actor package dirty and the change is not saved
        screen.static_mesh_component.set_static_mesh(mesh)
        screen.set_actor_location_and_rotation(unreal.Vector(x, y, z), unreal.Rotator(0, 0, 0), False, False)
        screen.set_actor_scale3d(unreal.Vector(1, 1, 1))
        screen.set_editor_property("tags", [unreal.Name(t) for t in tags if not t.startswith("ScreenFrom:")]
                                   + [unreal.Name(f"ScreenFrom:{start[0]:.0f},{start[1]:.0f}")])
        bl.log(f"{map_path}: Piccadilly screen wrapped, pivot ({x / 100:.1f}, {y / 100:.1f}) m")
    bl.save(world, map_path)
    unreal.EditorLoadingAndSavingUtils.save_packages([s.get_outermost() for s in screens], False)
