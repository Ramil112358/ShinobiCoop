"""Downloads the London map's Sketchfab models (CC-BY / CC0) as GLB into SourceArt/World/London/Sketchfab/<key>.glb
(gitignored) and writes credits.json beside them (name, author, licence, URL) for docs/world/asset-credits.md.

  python Scripts/london/fetch_models.py

Needs a Sketchfab API token (Settings -> Password & API) in the SKETCHFAB_TOKEN environment variable; on Windows it is
also read from the user environment in the registry, so a `setx` works without restarting the shell. The token is only
sent to api.sketchfab.com and never printed or written. Models already downloaded are skipped.
Rejected (aerial photogrammetry with city tiles baked in, unclear provenance): London Eye 7fc09e8d, The Shard
31d6da87, The Gherkin 3bcfc1b1 -- built procedurally instead.
"""
import json
import os
import sys
import time
import urllib.error
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "SourceArt", "World", "London", "Sketchfab")
API = "https://api.sketchfab.com/v3/models/"
ALLOWED = ("CC Attribution", "CC0", "Public Domain")
MODELS = {
    "bus": "a2a2736856a7452bbbaf712e07ed8eb4",  # modern London double-decker (the AEC look-alike read as 1930s)
    "black_cab": "9e57abf043744b86adbbc96083da637b",
    "big_ben": "9a1c691fac774bcca4e2ba565c4c9d9b",
    "st_pauls": "21e41040c4ca45c7b134aa768b5c993b",
    "nelsons_column": "6f42acbc26c3451aba570db6621fa19c",
    "narrowboat": "bfd9e2f43e184c8398677f1744ff278a",
    "traffic_light": "75e09798f86247b69ce3ca150522d6cf",
    "transit_van": "4d7e7de43c6149448d13881f06e903d5",
    "sprinter_van": "f69de1315bb049c8946d57f6006acd73",
    "mini": "b262744901a04cda823f17289d1a4847",
    "prius": "04eaf6b288cc466785365b7d985e3b29",
    "camel_bench": "ec4cc34bee7242fb8b660e147b952bee",
    "cab_shelter": "e2f1342d70d54eb2867cb58c7dd57938",
    "phone_box": "66ab8d583ca24a0fa20f6096f66396b9",  # K6 kiosk, door closed (the Fab booth's door is modelled open)
}


def token():
    value = os.environ.get("SKETCHFAB_TOKEN")
    if not value and sys.platform == "win32":
        import winreg
        try:
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, "Environment") as key:
                value = winreg.QueryValueEx(key, "SKETCHFAB_TOKEN")[0]
        except OSError:
            value = None
    if not value:
        sys.exit("SKETCHFAB_TOKEN is not set (Sketchfab Settings -> Password & API, then setx SKETCHFAB_TOKEN ...)")
    return value


def get_json(url, auth=None):
    headers = {"User-Agent": "ShinobiCoop-dev"}
    if auth:
        headers["Authorization"] = f"Token {auth}"
    with urllib.request.urlopen(urllib.request.Request(url, headers=headers), timeout=60) as r:
        return json.loads(r.read())


def download(url, path):
    with urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": "ShinobiCoop-dev"}), timeout=600) as r, \
            open(path + ".part", "wb") as f:
        while chunk := r.read(1 << 20):
            f.write(chunk)
    os.replace(path + ".part", path)


def main():
    auth = token()
    os.makedirs(OUT, exist_ok=True)
    credits_path = os.path.join(OUT, "credits.json")
    credits = json.load(open(credits_path, encoding="utf-8")) if os.path.exists(credits_path) else {}
    for key, uid in MODELS.items():
        path = os.path.join(OUT, f"{key}.glb")
        info = get_json(API + uid)
        licence = (info.get("license") or {}).get("label", "")
        if not licence.startswith(ALLOWED) or "NonCommercial" in licence or "NoDerivs" in licence:
            print(f"[fetch_models] {key}: licence '{licence}' not allowed, skipped")
            continue
        credits[key] = {"name": info["name"], "author": info["user"]["displayName"] or info["user"]["username"],
                        "licence": licence, "url": info["viewerUrl"]}
        if os.path.exists(path):
            print(f"[fetch_models] {key} cached")
            continue
        try:
            links = get_json(API + uid + "/download", auth)
        except urllib.error.HTTPError as e:  # 429: Sketchfab's download rate limit; run again later
            print(f"[fetch_models] {key}: HTTP {e.code}, skipped (run again later)")
            continue
        if "glb" not in links:
            print(f"[fetch_models] {key}: no GLB offered ({', '.join(links)}), skipped")
            continue
        download(links["glb"]["url"], path)
        print(f"[fetch_models] {key}: {os.path.getsize(path) / 1e6:.1f} MB ({info['name']}, {licence})")
        time.sleep(1)
    with open(credits_path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(credits, f, indent=1, ensure_ascii=False)


if __name__ == "__main__":
    main()
