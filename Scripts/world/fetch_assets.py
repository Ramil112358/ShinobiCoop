"""Downloads the CC0 art packs for the open world into SourceArt/World/<Pack>/ (sub-project #3, plan Task 3).

  python Scripts/world/fetch_assets.py [pack ...]

Free itch.io packs go through itch's public "No thanks, just take me to the downloads" flow (no account): the purchase
page's CSRF token -> download_url -> the download page's upload ids -> file/<id> (no key needed for free packs) ->
a CDN URL valid for 60 s, fetched straight away. Each zip is
extracted once and skipped on later runs. Licences and sources are listed in docs/world/asset-credits.md.
Standard library only.
"""
import http.cookiejar
import json
import os
import re
import sys
import urllib.parse
import urllib.request
import zipfile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "SourceArt", "World")

# name -> (itch page, regex picking the upload to download)
ITCH_PACKS = {
    "QuaterniusNature": ("https://quaternius.itch.io/stylized-nature-megakit", r"Standard"),
    "QuaterniusVillage": ("https://quaternius.itch.io/medieval-village-megakit", r"Standard"),
    "QuaterniusFantasyProps": ("https://quaternius.itch.io/fantasy-props-megakit", r"Standard"),
}

jar = http.cookiejar.CookieJar()
opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar))
opener.addheaders = [("User-Agent", "Mozilla/5.0 (ShinobiCoop asset fetch)")]


def get(url):
    with opener.open(url) as response:
        return response.read().decode("utf-8", "replace")


def post_json(url, csrf):
    data = urllib.parse.urlencode({"csrf_token": csrf}).encode()
    request = urllib.request.Request(url, data=data, headers={"X-Requested-With": "XMLHttpRequest"})
    with opener.open(request) as response:
        return json.loads(response.read().decode())


def csrf_of(html):
    match = re.search(r'name="csrf_token" (?:value|content)="([^"]+)"', html) or re.search(r'csrf_token" value="([^"]+)"', html)
    if not match:
        raise RuntimeError("no CSRF token on the page")
    return match.group(1)


def fetch_itch(name, page, pick):
    out_dir = os.path.join(ROOT, name)
    if os.path.isdir(out_dir) and os.listdir(out_dir):
        print(f"[fetch] {name}: already there")
        return
    purchase = get(page + "/purchase")
    download_page_url = post_json(page + "/download_url", csrf_of(purchase))["url"]
    download_page = get(download_page_url)
    uploads = re.findall(r'data-upload_id="(\d+)".*?class="name"[^>]*>([^<]+)<', download_page, re.S)
    chosen = [(uid, fname) for uid, fname in uploads if re.search(pick, fname)] or uploads[:1]
    if not chosen:
        raise RuntimeError(f"{name}: no downloadable files on {download_page_url}")
    uid, fname = chosen[0]
    cdn = post_json(f"{page}/file/{uid}?source=game_download", csrf_of(purchase))
    if "url" not in cdn:
        raise RuntimeError(f"{name}: itch refused the download: {cdn}")
    os.makedirs(out_dir, exist_ok=True)
    zip_path = os.path.join(ROOT, f"{name}.zip")
    print(f"[fetch] {name}: downloading {fname}")
    with opener.open(cdn["url"]) as response, open(zip_path, "wb") as f:
        while chunk := response.read(1 << 20):
            f.write(chunk)
    with zipfile.ZipFile(zip_path) as archive:
        archive.extractall(out_dir)
    os.remove(zip_path)
    print(f"[fetch] {name}: extracted to {out_dir}")


if __name__ == "__main__":
    wanted = sys.argv[1:] or list(ITCH_PACKS)
    failed = []
    for pack in wanted:
        try:
            fetch_itch(pack, *ITCH_PACKS[pack])
        except Exception as error:  # a missing pack is a warning: the world build falls back to placeholders
            print(f"[fetch] WARNING {pack}: {error}")
            failed.append(pack)
    sys.exit(1 if failed and len(failed) == len(wanted) else 0)
