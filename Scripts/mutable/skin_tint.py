"""The body tint that matches a grafted head's skin (system Python with Pillow; run by Scripts/setup_mutable.py heads):
the neck texture's mean linear colour over the base body texture's, per channel, printed as JSON [r, g, b].

  python Scripts/mutable/skin_tint.py <head_neck.png> <body_base_colour.png>
"""
import json
import sys

from PIL import Image


def mean(path):
    pixels = list(Image.open(path).convert("RGB").resize((64, 64)).getdata())
    return [sum((p[i] / 255) ** 2.2 for p in pixels) / len(pixels) for i in range(3)]


# ponytail: plain texture ratio; MetaHuman skin adds subsurface scatter in-engine, so the body renders a little warmer than
# its baked base colour. A per-channel scatter factor gets calibrated from the roster side-by-side shots if seams show.
neck, body = mean(sys.argv[1]), mean(sys.argv[2])
print(json.dumps([round(min(max(neck[i] / max(body[i], 1e-4), 0.25), 4.0), 4) for i in range(3)]))
