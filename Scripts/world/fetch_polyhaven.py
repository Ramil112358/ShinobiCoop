"""Downloads the realistic world's Poly Haven assets (CC0) listed in polyhaven_assets.json into
SourceArt/World/PolyHaven/<id>/ (gitignored). Standard-library Python: `python Scripts/world/fetch_polyhaven.py [list.json]`.

Models come as glTF (the .gltf, its .bin and its JPG textures, laid out as the .gltf references them); textures as
JPG albedo, OpenGL normal, roughness and displacement. Files whose size already matches the API are skipped, and each
download goes to <name>.part first, so an interrupted run resumes cleanly. import_library.py imports the result."""
import json
import os
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "SourceArt", "World", "PolyHaven")
API = "https://api.polyhaven.com/files/"
AGENT = {"User-Agent": "ShinobiCoop-dev"}
TEXTURE_MAPS = ("Diffuse", "nor_gl", "Rough", "Displacement")


def plan_downloads(files, res, kind):
    """[(url, path relative to the asset folder, size)] for one asset's API `files` JSON."""
    if kind == "model":
        gltf = files["gltf"][res]["gltf"]
        plan = [(gltf["url"], gltf["url"].rsplit("/", 1)[-1], gltf["size"])]
        plan += [(inc["url"], rel, inc["size"]) for rel, inc in gltf["include"].items()]
        # The glTF's colour maps are JPGs (no alpha): foliage needs its separate alpha maps (Alpha, twig_alpha, ...).
        for key, levels in files.items():
            if key.lower().endswith("alpha") and res in levels:
                jpg = levels[res]["jpg"]
                plan.append((jpg["url"], "textures/" + jpg["url"].rsplit("/", 1)[-1], jpg["size"]))
        return plan
    plan = []
    for name in TEXTURE_MAPS:
        jpg = files[name][res]["jpg"]
        plan.append((jpg["url"], jpg["url"].rsplit("/", 1)[-1], jpg["size"]))
    return plan


def needs_download(path, size):
    return not os.path.exists(path) or os.path.getsize(path) != size


def get(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=AGENT), timeout=120) as r:
        return r.read()


def download(url, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with urllib.request.urlopen(urllib.request.Request(url, headers=AGENT), timeout=120) as r, open(path + ".part", "wb") as f:
        while chunk := r.read(1 << 20):
            f.write(chunk)
    os.replace(path + ".part", path)


def main():
    # Optional argument: another list in the same format (Scripts/london/polyhaven_london.json).
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "polyhaven_assets.json")
    with open(path, encoding="utf-8") as f:
        assets = json.load(f)
    total = fetched = 0
    for kind, group in (("model", assets["models"]), ("texture", assets["textures"])):
        for asset_id, spec in group.items():
            files = json.loads(get(API + asset_id))
            for url, rel, size in plan_downloads(files, spec["res"], kind):
                path = os.path.join(OUT, asset_id, rel)
                total += size
                if needs_download(path, size):
                    download(url, path)
                    fetched += size
                    if os.path.getsize(path) != size:
                        sys.exit(f"size mismatch for {path}")
            print(f"[fetch_polyhaven] {asset_id} ok")
    print(f"[fetch_polyhaven] {total / 1e9:.2f} GB total, {fetched / 1e9:.2f} GB downloaded this run")


if __name__ == "__main__":
    main()
