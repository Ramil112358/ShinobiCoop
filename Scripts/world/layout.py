"""Loads and validates Scripts/world/world_layout.json, the single source for the open world (sub-project #3).

Coordinates are metres from the map centre, +X east, +Y north. Plain Python (no numpy), so UE's embedded Python can
import it too.
"""
import json
import os
from dataclasses import dataclass, field

DEFAULT_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "world_layout.json")
PLAY_HALF_EXTENT_M = 450.0  # everything playable lies inside +/-450 m; mountains beyond


@dataclass
class Camp:
    id: str
    x: float
    y: float
    r: float
    difficulty: int
    waves: list  # [{"ninjas": int, "throwers": int}]


@dataclass
class Layout:
    size_m: float
    areas: dict  # name -> (x, y, r)
    paths: list  # [[(x, y), ...], ...]
    river: list  # [(x, y), ...]
    bridge: tuple  # (x, y, yaw)
    buildings: list  # [{"type", "x", "y", "yaw"}]
    camps: list = field(default_factory=list)
    boss: dict = field(default_factory=dict)  # {"x", "y", "r"}
    villagers: list = field(default_factory=list)  # [[(x, y), ...], ...]
    village_name: str = ""


def _inside(x, y, what):
    if abs(x) > PLAY_HALF_EXTENT_M or abs(y) > PLAY_HALF_EXTENT_M:
        raise ValueError(f"{what} at ({x}, {y}) is outside the playable +/-{PLAY_HALF_EXTENT_M:.0f} m")


def load(path=DEFAULT_PATH):
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    return parse(data)


def parse(data):
    areas = {name: tuple(v) for name, v in data["areas"].items()}
    camps, seen = [], set()
    for c in data["camps"]:
        if c["id"] in seen:
            raise ValueError(f"duplicate camp id {c['id']}")
        seen.add(c["id"])
        _inside(c["x"], c["y"], f"camp {c['id']}")
        if not 1 <= c["difficulty"] <= 5 or not c["waves"]:
            raise ValueError(f"camp {c['id']}: difficulty must be 1-5 and it needs at least one wave")
        camps.append(Camp(c["id"], float(c["x"]), float(c["y"]), float(c["r"]), int(c["difficulty"]), c["waves"]))
    boss = data["boss"]
    _inside(boss["x"], boss["y"], "boss")
    for b in data["buildings"]:
        _inside(b["x"], b["y"], f"building {b['type']}")
    return Layout(
        size_m=float(data["size_m"]),
        areas=areas,
        paths=[[tuple(p) for p in path] for path in data["paths"]],
        river=[tuple(p) for p in data["river"]],
        bridge=tuple(data["bridge"]),
        buildings=data["buildings"],
        camps=camps,
        boss=boss,
        villagers=[[tuple(p) for p in route] for route in data["villagers"]],
        village_name=data.get("village_name", ""),
    )
