"""Shop fascia signs for the London generator: one 2048 x 2048 atlas of 2 x 16 painted sign boards (1024 x 128 each,
the 8:1 shape of a shop fascia), lettered with the system's serif and sans fonts. All shop names are made up.

  python Scripts/london/make_signage.py [out.png]      (default SourceArt/World/London/T_Signage.png)

Cell n is column n % 2, row n // 2 from the top; Scripts/london/blender/make_city_meshes.py maps fascia faces onto it.
Deterministic. Needs Pillow.
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

COLS, ROWS, W, H = 2, 16, 1024, 128
FONTS = "C:/Windows/Fonts"
NAMES = [
    ("ALBION BOOKS", "est. 1898"), ("The Crown & Anchor", ""), ("MARLOWE & SONS", "Tailors"), ("Kettle & Crumb", "cafe"),
    ("HALBERD HARDWARE", ""), ("Primrose Florist", ""), ("THE OLD BELL", "free house"), ("Soho Records", "vinyl"),
    ("FENWICK OPTICIANS", ""), ("Pemberton's", "fine wines"), ("NORTHGATE NEWS", "& tobacco"), ("Lantern Noodle Bar", ""),
    ("BAXTER & CO", "estate agents"), ("Rosie's Bakery", ""), ("THE KINGS ARMS", ""), ("Chalk Farm Vintage", ""),
    ("HOLBORN PHARMACY", ""), ("Mile End Barbers", ""), ("GREY HERON GALLERY", ""), ("Thameside Fish Bar", ""),
    ("THE GLOBE", "ales & spirits"), ("Wren Stationers", ""), ("BELMONT SHOES", ""), ("Little Venice Pizza", ""),
    ("CLERKENWELL CYCLES", ""), ("Juniper Deli", ""), ("THE LOCK TAVERN", ""), ("Orchard Grocers", ""),
    ("ST JAMES TEAS", ""), ("Brixton Tattoo", ""), ("MAYFAIR JEWELLERS", ""), ("The Fox & Hound", ""),
]
# (board colour, letter colour, font file)
SCHEMES = [
    ((20, 48, 34), (214, 182, 104), "georgiab.ttf"), ((18, 24, 56), (235, 228, 210), "GIL_____.TTF"),
    ((12, 12, 12), (212, 175, 90), "ENGR.TTF"), ((92, 14, 22), (240, 228, 200), "BOOKOSB.TTF"),
    ((226, 218, 196), (30, 34, 40), "GILB____.TTF"), ((32, 70, 78), (245, 240, 225), "georgia.ttf"),
    ((60, 30, 20), (222, 196, 130), "CENTAUR.TTF"), ((240, 240, 236), (150, 20, 30), "BRITANIC.TTF"),
]


def fit(draw, text, font_file, max_w, max_h):
    size = max_h
    while size > 10:
        font = ImageFont.truetype(os.path.join(FONTS, font_file), size)
        x0, y0, x1, y1 = draw.textbbox((0, 0), text, font=font)
        if x1 - x0 <= max_w and y1 - y0 <= max_h:
            return font, (x1 - x0, y1 - y0, y0)
        size -= 2
    return ImageFont.truetype(os.path.join(FONTS, font_file), 10), (0, 0, 0)


def main(out):
    atlas = Image.new("RGB", (COLS * W, ROWS * H), (0, 0, 0))
    for n, (name, sub) in enumerate(NAMES):
        board, ink, font_file = SCHEMES[(n * 5) % len(SCHEMES)]
        cell = Image.new("RGB", (W, H), board)
        d = ImageDraw.Draw(cell)
        d.rectangle([6, 6, W - 7, H - 7], outline=tuple(min(255, c + 35) for c in board), width=3)  # moulding line
        d.rectangle([14, 14, W - 15, H - 15], outline=ink, width=2)
        main_h = 70 if sub else 84
        font, (tw, th, oy) = fit(d, name, font_file, W - 120, main_h)
        y = (H - th) // 2 - oy - (12 if sub else 0)
        d.text(((W - tw) // 2, y), name, font=font, fill=ink)
        if sub:
            sfont, (sw, sh, soy) = fit(d, sub, "georgiai.ttf", 300, 24)
            d.text(((W - sw) // 2, H - 22 - sh - soy), sub, font=sfont, fill=ink)
        atlas.paste(cell, ((n % COLS) * W, (n // COLS) * H))
    atlas.save(out)
    print(f"[make_signage] {len(NAMES)} signs -> {out}")


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "SourceArt", "World", "London", "T_Signage.png"))
