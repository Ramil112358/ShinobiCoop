"""SpriteCook 2K sources -> the sheets setup_sprite_vfx.py imports (Chidori / Rasengan hand orbs, their ground marks).

  chidori_orb_2k.png   -> chidori_orb_sheet.png   4 x 2 atlas, 1024 px, each frame re-centred in its 256 x 512 cell
  rasengan_orb_2k.png  -> rasengan_orb_sheet.png  same
  impact_marks_2k.png  -> chidori_dent_rgba.png (left mark), rasengan_crater_rgba.png (right mark): 512 px RGBA,
                          alpha cut from the white background like scorch_rgba.png

Run: python Scripts/prep_jutsu_sprites.py  (Pillow + numpy)
"""
import os
import numpy as np
from PIL import Image

VFX = os.path.join(os.path.dirname(__file__), "..", "SourceArt", "SpriteCook", "VFX")


def recentre_sheet(src, dst):
    # The model drifts each frame off its cell centre; the cards assume the orb sits in the middle.
    img = np.asarray(Image.open(os.path.join(VFX, src)).convert("RGB")).astype(np.float32)
    h, w, _ = img.shape
    cw, ch = w // 4, h // 2
    out = np.zeros_like(img)
    lum = img.mean(axis=2)
    for row in range(2):
        for col in range(4):
            x0, y0 = col * cw, row * ch
            cell = lum[y0:y0 + ch, x0:x0 + cw]
            weight = np.where(cell > 60, cell, 0)  # the bright body, not faint haze
            ys, xs = np.mgrid[0:ch, 0:cw]
            cx = int(round((weight * xs).sum() / weight.sum()))
            cy = int(round((weight * ys).sum() / weight.sum()))
            dx, dy = cw // 2 - cx, ch // 2 - cy
            shifted = np.zeros((ch, cw, 3), np.float32)
            sx0, sx1 = max(0, -dx), min(cw, cw - dx)
            sy0, sy1 = max(0, -dy), min(ch, ch - dy)
            shifted[sy0 + dy:sy1 + dy, sx0 + dx:sx1 + dx] = img[y0 + sy0:y0 + sy1, x0 + sx0:x0 + sx1]
            out[y0:y0 + ch, x0:x0 + cw] = shifted
            print(f"{src} cell {col},{row}: shifted {dx:+d},{dy:+d}")
    Image.fromarray(out.clip(0, 255).astype(np.uint8)).resize((1024, 1024), Image.LANCZOS).save(os.path.join(VFX, dst))


def cut_mark(img, dst):
    rgb = img.astype(np.float32) / 255.0
    lum = rgb.mean(axis=2)
    ys, xs = np.nonzero(lum < 0.85)
    (x0, x1), (y0, y1) = np.percentile(xs, [0.5, 99.5]).astype(int), np.percentile(ys, [0.5, 99.5]).astype(int)  # ignore stray specks
    cx, cy = (x0 + x1) // 2, (y0 + y1) // 2
    half = int(max(x1 - x0, y1 - y0) * 0.6)
    pad = np.ones((rgb.shape[0] + 2 * half, rgb.shape[1] + 2 * half, 3), np.float32)
    pad[half:half + rgb.shape[0], half:half + rgb.shape[1]] = rgb
    crop = pad[cy:cy + 2 * half, cx:cx + 2 * half]
    lum = crop.mean(axis=2)
    # Over white: C = a * F + (1 - a). Darker = more opaque; recover F from C.
    alpha = np.clip((1.0 - lum) * 1.8, 0.0, 1.0)
    colour = np.clip((crop - (1.0 - alpha[..., None])) / np.maximum(alpha[..., None], 1e-3), 0.0, 1.0)
    # Feather to nothing before the square's edge, so the plane never shows a border.
    yy, xx = np.mgrid[-1:1:crop.shape[0] * 1j, -1:1:crop.shape[1] * 1j]
    alpha *= np.clip((1.0 - np.sqrt(xx ** 2 + yy ** 2)) / 0.12, 0.0, 1.0)
    rgba = np.dstack([colour, alpha])
    Image.fromarray((rgba * 255).astype(np.uint8)).resize((512, 512), Image.LANCZOS).save(os.path.join(VFX, dst))
    print(f"{dst}: centre {cx},{cy} half {half}")


if __name__ == "__main__":
    recentre_sheet("chidori_orb_2k.png", "chidori_orb_sheet.png")
    recentre_sheet("rasengan_orb_2k.png", "rasengan_orb_sheet.png")
    marks = np.asarray(Image.open(os.path.join(VFX, "impact_marks_2k.png")).convert("RGB"))
    # The two marks are not split at the middle: cut at the emptiest column between them.
    dark = (marks.mean(axis=2) < 200).sum(axis=0)
    mid = 600 + int(np.argmin(dark[600:1200]))
    cut_mark(marks[:, :mid], "chidori_dent_rgba.png")
    cut_mark(marks[:, mid:], "rasengan_crater_rgba.png")
