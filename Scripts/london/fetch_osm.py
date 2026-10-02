"""Downloads the OpenStreetMap data (ODbL, (c) OpenStreetMap contributors) for each district of london_layout.json
into SourceArt/World/London/osm/<id>.json (gitignored). Standard-library Python: `python Scripts/london/fetch_osm.py`.

One Overpass query per district: its cut-out plus margin_m all round (the backdrop ring). Cached: a district whose
file exists is skipped (delete it to refetch). Water relations come back whole (out geom), so the Thames polygon can
be rebuilt from its member ways. Falls back to a mirror when the main Overpass server is busy.
"""
import json
import math
import os
import sys
import time
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "SourceArt", "World", "London", "osm")
SERVERS = ("https://overpass-api.de/api/interpreter", "https://overpass.kumi.systems/api/interpreter")
AGENT = {"User-Agent": "ShinobiCoop-dev (London map build)"}
QUERY = """[out:json][timeout:240];
(
  way["building"]({b}); relation["building"]({b}); way["building:part"]({b}); relation["building:part"]({b});
  way["highway"]({b}); way["area:highway"]({b});
  way["natural"="water"]({b}); relation["natural"="water"]({b}); way["waterway"]({b}); way["man_made"~"bridge|pier"]({b});
  way["leisure"~"park|garden|pitch|playground"]({b}); relation["leisure"~"park|garden"]({b});
  way["landuse"~"grass|recreation_ground|village_green|flowerbed"]({b}); way["natural"~"tree_row|scrub|wood"]({b});
  way["place"="square"]({b}); way["barrier"~"wall|fence|railing|retaining_wall"]({b}); way["historic"]({b});
  node["natural"="tree"]({b});
  node["highway"~"crossing|traffic_signals|street_lamp|bus_stop"]({b});
  node["amenity"~"telephone|post_box|bench|waste_basket|bicycle_parking|bicycle_rental"]({b});
  node["shop"]({b}); node["amenity"~"pub|restaurant|cafe|bar|fast_food"]({b});
);
out geom;"""


def bbox(district, margin_m):
    """south,west,north,east of the cut-out plus margin (equirectangular; error < 0.1% over a few km)."""
    half_ew, half_ns = district["size_m"][0] / 2 + margin_m, district["size_m"][1] / 2 + margin_m
    dlat = half_ns / 111_320.0
    dlon = half_ew / (111_320.0 * math.cos(math.radians(district["lat"])))
    return (district["lat"] - dlat, district["lon"] - dlon, district["lat"] + dlat, district["lon"] + dlon)


def query(text):
    data = urllib.parse.urlencode({"data": text}).encode()
    last = None
    for attempt in range(3):
        for server in SERVERS:
            try:
                with urllib.request.urlopen(urllib.request.Request(server, data=data, headers=AGENT), timeout=300) as r:
                    return r.read()
            except Exception as e:  # busy (429/504) or timed out: next server, then back off
                last = e
                print(f"[fetch_osm] {server}: {e}")
        time.sleep(20 * (attempt + 1))
    raise RuntimeError(f"Overpass failed: {last}")


def main():
    with open(os.path.join(HERE, "london_layout.json"), encoding="utf-8") as f:
        layout = json.load(f)
    os.makedirs(OUT, exist_ok=True)
    for district in layout["districts"]:
        path = os.path.join(OUT, f"{district['id']}.json")
        if os.path.exists(path):
            print(f"[fetch_osm] {district['id']} cached")
            continue
        b = ",".join(f"{v:.6f}" for v in bbox(district, layout["margin_m"]))
        raw = query(QUERY.replace("{b}", b))
        elements = json.loads(raw)["elements"]  # fails loudly on an error page
        with open(path + ".part", "wb") as f:
            f.write(raw)
        os.replace(path + ".part", path)
        print(f"[fetch_osm] {district['id']}: {len(elements)} elements, {len(raw) / 1e6:.1f} MB")
        time.sleep(5)  # be polite to the public server


if __name__ == "__main__":
    sys.exit(main())
