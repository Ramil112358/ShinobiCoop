"""Feathers the top edge of a photo-baked face patch on a MetaHuman head base colour (system Python: numpy + Pillow).

The face bake (bake_face_video.py) covers the face up to the upper forehead; its outline there shows in game as a thin
dark line with a step in skin tone above it (BikaMH). The edge is traced column by column across the forehead (the
strongest light-below step of the smoothed luminance, in the UV band where the MetaHuman head layout puts the
forehead), smoothed; above it the skin is shifted to the colour just below it (fading out upward over FADE, so the
texture's detail stays), and a thin strip on it is blended into a blurred copy (the rim).

  python Scripts/character/blend_bake_edge.py <in.png> <out.png> [--check]

Works at any resolution of the head layout (coordinates are fractions of the size). --check prints the traced edge.
"""
import sys

import numpy as np
from PIL import Image, ImageFilter

X_SPAN = (0.24, 0.76)  # forehead and temple columns (MetaHuman head UVs)
Y_SPAN = (0.18, 0.38)  # where the bake's top edge can be
RIM = 0.004  # half-width of the blurred strip on the edge, as a fraction of the size
SAMPLE = (0.004, 0.016)  # rows either side of the edge whose colours are compared
FADE = 0.06  # how far above the edge the colour shift fades out


def trace(lum, size):
    """The edge's row per column in X_SPAN (median-smoothed), or None for columns without a clear step."""
    x0, x1 = (int(f * size) for f in X_SPAN)
    y0, y1 = (int(f * size) for f in Y_SPAN)
    step = lum[y0 + 2:y1, x0:x1] - lum[y0:y1 - 2, x0:x1]  # light below a dark rim: positive
    rows = step.argmax(axis=0) + y0 + 1
    strength = step.max(axis=0)
    rows = np.where(strength > 0.5 * np.median(strength), rows, -1).astype(float)
    good = rows >= 0
    xs = np.arange(x0, x1)
    rows = np.interp(xs, xs[good], rows[good])
    k = max(3, int(0.01 * size)) | 1
    padded = np.pad(rows, k // 2, mode="edge")
    return xs, np.array([np.median(padded[i:i + k]) for i in range(len(rows))])


def main():
    src, out = sys.argv[1], sys.argv[2]
    image = Image.open(src)
    mode = image.mode
    rgb = image.convert("RGB")
    size = rgb.size[0]
    lum = np.asarray(rgb.convert("L").filter(ImageFilter.GaussianBlur(max(1, size // 1024))), dtype=float)
    xs, rows = trace(lum, size)
    if "--check" in sys.argv:
        print({"size": size, "edge rows (every 10%)": [int(r) for r in rows[::max(1, len(rows) // 10)]]})
    pixels = np.asarray(rgb, dtype=float)
    yy = np.arange(size)[:, None]
    dist = yy - rows[None, :]  # rows below the edge: positive
    # the colour step per column (above vs below the edge), smoothed along the edge
    near, far = (int(f * size) for f in SAMPLE)
    region = pixels[:, xs]
    above = ((dist <= -near) & (dist >= -far))[..., None]
    below = ((dist >= near) & (dist <= far))[..., None]
    delta = (region * below).sum(0) / below.sum(0) - (region * above).sum(0) / above.sum(0)
    k = max(3, int(0.02 * size)) | 1
    delta = np.stack([np.convolve(np.pad(delta[:, c], k // 2, mode="edge"), np.ones(k) / k, "valid") for c in range(3)], 1)
    # fade it at the ends of the traced span, so nothing stops in a straight line
    ends = np.clip(np.minimum(xs - xs[0], xs[-1] - xs) / (0.05 * size), 0, 1)
    up = np.clip(1 + dist / (FADE * size), 0, 1) * (dist < 0) + (dist >= 0) * np.clip(1 - dist / near, 0, 1)
    up = up * up * (3 - 2 * up)
    result = pixels.copy()
    result[:, xs] += up[..., None] * delta[None] * ends[None, :, None]
    # the rim: a thin strip blended into a blur
    blurred = np.asarray(Image.fromarray(np.clip(result, 0, 255).astype(np.uint8)).filter(
        ImageFilter.GaussianBlur(RIM * size)), dtype=float)
    weight = np.zeros(lum.shape)
    weight[:, xs] = np.clip(1 - np.abs(dist) / (2 * RIM * size), 0, 1) * ends[None, :]
    weight = weight * weight * (3 - 2 * weight)
    result = result * (1 - weight[..., None]) + blurred * weight[..., None]
    done = Image.fromarray(np.clip(result + 0.5, 0, 255).astype(np.uint8))
    (done.convert(mode) if mode in ("RGBA", "L") else done).save(out)


if __name__ == "__main__":
    main()
