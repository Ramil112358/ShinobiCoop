"""In-game collision probe for an arena level (build_arenas.py). Static mesh collision of a reloaded World Partition map is
absent in the Python commandlet, so the walls are checked in a real game session: a capsule (the player's size) swept
out through each wall at 1.5 m and 55 m must hit an InvisibleWall, and the ground at the centre must be a chunk mesh.
Run with the game:
  $env:SHINOBI_ARENA = "Westminster"
  UnrealEditor.exe ShinobiCoop.uproject /Game/Shinobi/Maps/Lvl_Arena_Westminster -game -ShinobiLookShot
      -ExecCmds="py <repo>/Scripts/london/probe_arena.py" -log=Probe.log
and read the [probe_arena] lines of Saved/Logs/Probe.log.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_arenas as ba  # noqa: E402


def log(message):
    unreal.log(f"[probe_arena] {message}")


def owner_of(hit):
    fields = hit.to_tuple()
    component = next((v for v in fields if isinstance(v, unreal.PrimitiveComponent)), None)
    location = next(v for v in fields if isinstance(v, unreal.Vector))
    return component, location


def main():
    arena = next(a for a in ba.load_arenas() if a["id"] == os.environ["SHINOBI_ARENA"])
    world = unreal.find_object(None, f"{arena['map']}.{arena['map'].split('/')[-1]}")
    x0, y0, x1, y1 = (ba.uu(v) for v in ba.map_rect(arena["rect"]))
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    statics = [unreal.ObjectTypeQuery.ECC_WORLD_STATIC]
    found, report = 0, []
    for z in (150, 5500):
        for side, a, b in (("N", (cx, y0 + 1000), (cx, y0 - 1000)), ("S", (cx, y1 - 1000), (cx, y1 + 1000)),
                           ("W", (x0 + 1000, cy), (x0 - 1000, cy)), ("E", (x1 - 1000, cy), (x1 + 1000, cy))):
            hits = unreal.SystemLibrary.capsule_trace_multi_for_objects(
                world, unreal.Vector(a[0], a[1], z), unreal.Vector(b[0], b[1], z), 42, 96, statics, False, [],
                unreal.DrawDebugTrace.NONE, True) or []
            wall = any(c and str(c.get_collision_profile_name()) == "InvisibleWall" for c, _ in map(owner_of, hits))
            found += wall
            report.append(f"{side}@{z / 100:.0f}m {'wall' if wall else 'NONE'}")
    log(f"{arena['id']}: {found}/8 capsule sweeps hit an InvisibleWall ({', '.join(report)})")
    hit = unreal.SystemLibrary.line_trace_single_for_objects(world, unreal.Vector(cx, cy, 20000), unreal.Vector(cx, cy, -2000),
                                                             statics, False, [], unreal.DrawDebugTrace.NONE, True)
    if hit:
        component, location = owner_of(hit)
        log(f"{arena['id']}: ground at the centre z {location.z / 100:.2f} m on {component.get_owner().get_name() if component else '?'}")
    else:
        log(f"{arena['id']}: NO ground at the centre")


main()
