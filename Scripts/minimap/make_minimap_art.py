"""Render deterministic minimap artwork from world source data, then split it into map-coordinate tiles."""
import argparse
import json
import math
import os

import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CONFIG_PATH = os.path.join(os.path.dirname(__file__), "minimap_maps.json")
OUT_ROOT = os.path.join(ROOT, "SourceArt", "World", "Minimaps")


def load_json(path):
    with open(path, encoding="utf-8") as stream:
        return json.load(stream)


def dimensions(config, edge):
    low, high = config["world_min"], config["world_max"]
    width, height = high[0] - low[0], high[1] - low[1]
    scale = edge / max(width, height)
    return max(1, round(width * scale)), max(1, round(height * scale))


def xy(config, size, point):
    """A source-data point (source_units_cm) to artwork pixels: +X right, +Y down (UE's unmirrored top-down view)."""
    low, high = config["world_min"], config["world_max"]
    units = config.get("source_units_cm", 1)
    u = (point[0] * units - low[0]) / (high[0] - low[0])
    v = (point[1] * units - low[1]) / (high[1] - low[1])
    return (round(u * (size[0] - 1)), round(v * (size[1] - 1)))


def alpha_colour(base, colour, mask):
    layer = Image.new("RGB", base.size, colour)
    return Image.composite(layer, base, mask)


def valley_art(config, size):
    terrain_dir = os.path.join(ROOT, "SourceArt", "World", "Terrain")
    channels = ["grass", "forest", "dirt", "rock", "bank"]
    palette = np.asarray([(139, 158, 119), (79, 111, 83), (166, 135, 94), (119, 122, 116), (192, 173, 130)], dtype=np.float32)
    weights = []
    for name in channels:
        mask = Image.open(os.path.join(terrain_dir, f"weight_{name}.png")).convert("L")
        mask = mask.resize(size, Image.Resampling.BILINEAR)
        weights.append(np.asarray(mask, dtype=np.float32) / 255.0)
    weight_array = np.stack(weights, axis=0)
    weight_array /= np.maximum(weight_array.sum(axis=0, keepdims=True), 1e-5)
    pixels = np.einsum("chw,cd->hwd", weight_array, palette).clip(0, 255).astype(np.uint8)
    base = Image.fromarray(pixels)
    draw = ImageDraw.Draw(base)
    transform = lambda p: xy(config, size, p)
    scale = size[0] * config["source_units_cm"] / (config["world_max"][0] - config["world_min"][0])  # pixels per metre

    layout = load_json(os.path.join(ROOT, "Scripts", "world", "world_layout.json"))
    # River banks and water, followed by the visible path surface with a darker edge.
    river = [transform(p) for p in layout["river"]]
    draw.line(river, fill=(198, 183, 143), width=max(3, round(17 * scale)), joint="curve")
    draw.line(river, fill=(111, 164, 177), width=max(2, round(13 * scale)), joint="curve")
    for path in layout["paths"]:
        points = [transform(p) for p in path]
        draw.line(points, fill=(107, 89, 67), width=max(3, round(10 * scale)), joint="curve")
        draw.line(points, fill=(208, 185, 143), width=max(2, round(6 * scale)), joint="curve")

    bridge = layout["bridge"]
    centre = (bridge[0], bridge[1])
    bridge_a, bridge_b = transform((centre[0] - 11, centre[1] - 11)), transform((centre[0] + 11, centre[1] + 11))
    draw.ellipse((min(bridge_a[0], bridge_b[0]), min(bridge_a[1], bridge_b[1]),
                  max(bridge_a[0], bridge_b[0]), max(bridge_a[1], bridge_b[1])),
                 fill=(120, 104, 78), outline=(231, 214, 174), width=max(1, round(scale)))

    placements_path = os.path.join(terrain_dir, "placements.json")
    if os.path.exists(placements_path):
        placements = load_json(placements_path)
        for building in placements.get("buildings", []):
            centre = transform((building["x"] / 100.0, building["y"] / 100.0))
            radius = max(1, round(3.2 * scale))
            draw.rectangle((centre[0] - radius, centre[1] - radius, centre[0] + radius, centre[1] + radius),
                           fill=(105, 83, 66), outline=(226, 202, 159))
    return base


def draw_polygons(draw, config, size, polygons, fill):
    for polygon in polygons or []:
        if not polygon:
            continue
        ring = polygon[0] if isinstance(polygon[0][0], (list, tuple)) else polygon
        points = [xy(config, size, point) for point in ring]
        if len(points) >= 3:
            draw.polygon(points, fill=fill)


def london_art(config, size):
    city_dir = os.path.join(ROOT, "SourceArt", "World", "London")
    city = load_json(os.path.join(city_dir, "city.json"))
    ground = Image.new("RGB", size, (173, 170, 158))
    grass_mask = Image.open(os.path.join(city_dir, "weight_grass.png")).convert("L")
    grass_mask = grass_mask.resize(size, Image.Resampling.BILINEAR)
    ground = alpha_colour(ground, (108, 133, 105), grass_mask)
    draw = ImageDraw.Draw(ground)
    chunks = city["chunks"]

    # Parks sit below roads, paving and the river so the street structure reads clearly.
    for chunk in chunks.values():
        draw_polygons(draw, config, size, chunk.get("paving_backdrop"), (159, 155, 145))
    for chunk in chunks.values():
        draw_polygons(draw, config, size, chunk.get("road_backdrop"), (111, 111, 108))
    for chunk in chunks.values():
        draw_polygons(draw, config, size, chunk.get("paving"), (194, 189, 176))
    for chunk in chunks.values():
        draw_polygons(draw, config, size, chunk.get("road"), (74, 78, 81))
    for chunk in chunks.values():
        for layer, fill in chunk.items():
            if layer.startswith("water_"):
                draw_polygons(draw, config, size, fill, (102, 157, 174))
    buildings = city["buildings"]
    style_colours = {
        "georgian_terrace": (194, 181, 157), "victorian_terrace": (166, 144, 126), "stucco": (207, 196, 169),
        "portland_stone": (194, 184, 160), "gothic": (145, 137, 123), "warehouse": (132, 117, 103),
        "modern_glass": (131, 159, 165), "concrete_modern": (159, 159, 151),
    }
    for building in buildings:
        fill = style_colours.get(building.get("style"), (176, 166, 146))
        if building.get("zone") != "play":
            fill = tuple(round(c * 0.72 + 255 * 0.28) for c in fill)
        draw_polygons(draw, config, size, building.get("rings", []), fill)
    return ground


def london_crop_art(config, size):
    """An arena's rectangle cut from the London artwork (the arenas keep London's world coordinates)."""
    london = load_json(CONFIG_PATH)["maps"]["London"]
    art = Image.open(os.path.join(OUT_ROOT, "London", "artwork.png")).convert("RGB")
    low, high = london["world_min"], london["world_max"]
    box = tuple((config[key][axis] - low[axis]) / (high[axis] - low[axis]) * art.size[axis]
                for key in ("world_min", "world_max") for axis in (0, 1))
    return art.resize(size, Image.Resampling.LANCZOS, box=box)


def existing_art(size, name):
    path = os.path.join(OUT_ROOT, name, "artwork.png")
    if not os.path.exists(path):
        raise RuntimeError(f"{name}: missing {path}")
    image = Image.open(path).convert("RGB")
    if image.size != size:
        raise RuntimeError(f"{name}: artwork is {image.size}, minimap_maps.json wants {size}")
    return image


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--map", required=True, choices=sorted(load_json(CONFIG_PATH)["maps"]))
    parser.add_argument("--retile", action="store_true", help="re-tile the existing artwork.png instead of redrawing it")
    args = parser.parse_args()
    config = load_json(CONFIG_PATH)["maps"][args.map]
    size = dimensions(config, config["pixels_long_edge"])
    if args.retile or config["source"] == "artwork":
        image = existing_art(size, args.map)
    elif config["source"] == "valley":
        image = valley_art(config, size)
    elif config["source"] == "london":
        image = london_art(config, size)
    else:
        image = london_crop_art(config, size)

    output = os.path.join(OUT_ROOT, args.map)
    tile_size = load_json(CONFIG_PATH)["tile_size_px"]
    os.makedirs(output, exist_ok=True)
    image.save(os.path.join(output, "artwork.png"), optimize=True)
    columns, rows = math.ceil(size[0] / tile_size), math.ceil(size[1] / tile_size)
    tiles = []
    for y in range(rows):
        for x in range(columns):
            left, top = x * tile_size, y * tile_size
            right, bottom = min(left + tile_size, size[0]), min(top + tile_size, size[1])
            path = os.path.join(output, f"tile_{x}_{y}.png")
            image.crop((left, top, right, bottom)).save(path, optimize=True)
            tiles.append({
                "x": x, "y": y, "path": os.path.basename(path), "size": [right - left, bottom - top],
                "uv_min": [left / size[0], top / size[1]], "uv_max": [right / size[0], bottom / size[1]],
            })
    manifest = {
        "map": args.map, "image_size": list(size), "tile_count": [columns, rows], "tile_size_px": tile_size,
        "world_min": config["world_min"], "world_max": config["world_max"], "tiles": tiles,
    }
    with open(os.path.join(output, "manifest.json"), "w", encoding="utf-8", newline="\n") as stream:
        json.dump(manifest, stream, indent=2)
        stream.write("\n")
    print(f"[minimap_art] {args.map}: {size[0]}x{size[1]} pixels, {columns}x{rows} tiles")


if __name__ == "__main__":
    main()
