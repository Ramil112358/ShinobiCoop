"""Moves half of Lvl_London's crowd Londoners (marker tag Look:Crowd) onto the pavement walks near the player start, so
the City Sample crowd is seen where players arrive instead of spread thin over the whole city. One-off patch of the built
map (SourceArt/World/London, which make_city.py reads, is gone); close the GUI editor.

  powershell -File Scripts/run_python.ps1 -Script Scripts/london/cluster_crowd.py

The MOVE farthest crowd markers each take a copy of a route that starts within NEAR of the player start (a named
Londoner's or a near crowd member's), started at a different point round the loop so people on one walk are spread out.
Moved markers get the tag Cluster:Hub; the script does nothing when it finds one (already run).
"""
import math
import unreal

MAP = "/Game/Shinobi/Maps/Lvl_London"
NEAR = 25000.0  # cm from the player start
MOVE = 15


def log(message):
    unreal.log(f"[cluster_crowd] {message}")


def route_of(actor):
    tag = next((str(t) for t in actor.tags if str(t).startswith("Route:")), "")
    return [tuple(float(v) for v in p.split(",")) for p in tag[len("Route:"):].split("|") if p]


def main():
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(MAP)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    if any("Cluster:Hub" in [str(t) for t in a.tags] for a in actors):
        log("already clustered, nothing to do")
        return
    start = next(a for a in actors if isinstance(a, unreal.PlayerStart)).get_actor_location()
    dist = lambda p: math.hypot(p[0] - start.x, p[1] - start.y)
    walkers = [a for a in actors if "ShinobiVillager" in [str(t) for t in a.tags] and route_of(a)]
    crowd = sorted((a for a in walkers if "Look:Crowd" in [str(t) for t in a.tags]), key=lambda a: dist(route_of(a)[0]))
    near = [route_of(a) for a in walkers if dist(route_of(a)[0]) < NEAR]
    movers = crowd[-MOVE:]
    if len(near) < 2 or len(crowd) < 2 * MOVE:
        raise RuntimeError(f"{len(near)} near routes, {len(crowd)} crowd markers: not the London map this was written for")
    for j, actor in enumerate(movers):
        route = near[j % len(near)]
        # The k-th sharer of a walk starts k/(sharers+1) of the way round it (its owner starts at 0).
        sharers = len(movers[j % len(near)::len(near)])
        shift = (j // len(near) + 1) * len(route) // (sharers + 1)
        route = route[shift:] + route[:shift]
        x, y, z = route[0]
        actor.modify()
        tags = [t for t in actor.tags if not str(t).startswith("Route:")]
        tags += [unreal.Name("Route:" + "|".join(f"{px:.0f},{py:.0f},{pz:.0f}" for px, py, pz in route)), unreal.Name("Cluster:Hub")]
        actor.tags = tags
        actor.set_actor_location(unreal.Vector(x, y, z + 50), False, False)
        log(f"{actor.get_actor_label()}: {dist(route_of(actor)[0]) / 100:.0f} m from the start")
    unreal.EditorLoadingAndSavingUtils.save_packages([a.get_outermost() for a in movers], True)
    log(f"moved {len(movers)} of {len(crowd)} crowd markers onto {len(near)} walks within {NEAR / 100:.0f} m of the player start")


main()
