"""Tileable water normal map for the river (sub-project #3b): SourceArt/World/Water/T_WaterNormal.png, 1024 px.
Ripples are FFT-shaped noise (so the tile repeats seamlessly), turned into an OpenGL-style normal map (G = +Y).
System Python + numpy + Pillow; deterministic. build_world.py (stage "water") imports it."""
import os

import numpy as np
from PIL import Image

SIZE = 1024
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "SourceArt", "World", "Water", "T_WaterNormal.png")


def heightfield(seed=3):
    rng = np.random.default_rng(seed)
    f = np.fft.fftfreq(SIZE)
    k = np.hypot(*np.meshgrid(f, f))
    spectrum = np.fft.fft2(rng.standard_normal((SIZE, SIZE)))
    shape = np.exp(-((k - 0.02) / 0.015) ** 2) + 0.35 * np.exp(-((k - 0.06) / 0.03) ** 2)  # swells + small ripples
    h = np.real(np.fft.ifft2(spectrum * shape))
    return h / np.abs(h).max()


def normal_map(h, strength=6.0):
    gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * strength  # wraps: stays tileable
    gy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * strength
    n = np.dstack([-gx, gy, np.ones_like(h)])
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    return ((n * 0.5 + 0.5) * 255).round().astype(np.uint8)


if __name__ == "__main__":
    img = normal_map(heightfield())
    assert (img[0] .astype(int) - img[-1].astype(int)).std() < 40, "tile edges should match"
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    Image.fromarray(img).save(OUT)
    print(OUT)
