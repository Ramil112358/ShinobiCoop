"""Synthesises Lvl_London's city bed into SourceArt/Audio/World/Amb_City.wav (16-bit mono, seamless 30 s loop): distant
traffic rumble that swells, cars swishing past, a far-off horn, one siren gliding in the distance, pigeons cooing.
System Python + numpy, deterministic; reuses Scripts/world/make_ambience.py's helpers. Swap the file for a CC0 field
recording of the same name to upgrade it.
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "world"))
import make_ambience as amb  # noqa: E402

RATE = amb.RATE


def swish(seconds, rng):
    """A car passing: band noise whose centre slides down (approach, then away), shaped by a soft bell."""
    n = int(seconds * RATE)
    t = np.linspace(0, 1, n)
    noise = rng.standard_normal(n)
    out = np.zeros(n)
    for k, (lo, hi) in enumerate(((300, 1200), (200, 800), (120, 500))):
        band = np.fft.irfft(np.fft.rfft(noise) * ((np.fft.rfftfreq(n, 1 / RATE) > lo) & (np.fft.rfftfreq(n, 1 / RATE) < hi)), n)
        out += band * np.exp(-((t - (0.35 + 0.15 * k)) / 0.18) ** 2)
    return out / (np.abs(out).max() + 1e-9)


def tone(freq, seconds, harmonics=(1.0, 0.5, 0.25)):
    t = np.arange(int(seconds * RATE)) / RATE
    return sum(a * np.sin(2 * np.pi * freq * (i + 1) * t) for i, a in enumerate(harmonics)) * np.sin(np.pi * t / seconds) ** 0.5


def city(seconds=30.0):
    rng = np.random.default_rng(11)
    n = int(seconds * RATE)
    bed = amb.shaped_noise(seconds, 35, 450) * amb.gusts(n, (1, 3))
    bed /= np.abs(bed).max()
    out = bed * 0.55
    for i in range(9):  # cars passing
        add = swish(2.5 + rng.random() * 1.5, rng) * (0.18 + 0.12 * rng.random())
        amb.add_wrapped(out, add, int(rng.random() * n))
    for i in range(2):  # a far-off horn: two short blips
        blip = tone(420 + 30 * i, 0.22, (1.0, 0.6, 0.4, 0.2)) * 0.05
        start = int(rng.random() * n)
        amb.add_wrapped(out, blip, start)
        amb.add_wrapped(out, blip, start + int(0.3 * RATE))
    t = np.arange(int(7 * RATE)) / RATE  # one distant two-tone siren, faint
    siren = np.sin(2 * np.pi * np.cumsum(np.where((t % 1.4) < 0.7, 640.0, 820.0)) / RATE) * np.sin(np.pi * t / 7) * 0.025
    amb.add_wrapped(out, siren, int(0.4 * n))
    for i in range(5):  # pigeons: low cooing, a wobbling tone
        coo_t = np.arange(int(0.9 * RATE)) / RATE
        coo = np.sin(2 * np.pi * (380 + 60 * np.sin(2 * np.pi * 3 * coo_t)) * coo_t) * np.sin(np.pi * coo_t / 0.9) ** 2 * 0.03
        amb.add_wrapped(out, coo, int(rng.random() * n))
    return out


if __name__ == "__main__":
    amb.write("Amb_City", city())
