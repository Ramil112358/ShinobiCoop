"""Synthesises the open world's ambient loops into SourceArt/Audio/World as 16-bit mono WAVs (sub-project #3, W5):
Amb_Forest (wind gusts and bird calls) and Amb_River (running water and bubbles). System Python + numpy; deterministic.
The noise beds are shaped in the frequency domain, so each file loops seamlessly; events wrap around the loop point.
build_world.py (stage "mood") imports them and places the sounds. Swap either file for a CC0 recording of the same name."""
import os
import wave

import numpy as np

RATE = 44100
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "SourceArt", "Audio", "World")
rng = np.random.default_rng(7)


def shaped_noise(seconds, low, high):
    """Periodic noise band-limited to [low, high] Hz with a soft 1/f tilt."""
    n = int(seconds * RATE)
    spectrum = np.fft.rfft(rng.standard_normal(n))
    f = np.fft.rfftfreq(n, 1.0 / RATE)
    shape = 1.0 / (1.0 + ((f - (low + high) / 2) / ((high - low) / 2)) ** 4) / np.sqrt(np.maximum(f, 20.0))
    return np.fft.irfft(spectrum * shape, n)


def gusts(n, cycles):
    """A slow, loopable swell: a few whole sine cycles over the loop."""
    t = np.arange(n) / n
    return 0.55 + 0.25 * np.sin(2 * np.pi * cycles[0] * t) + 0.2 * np.sin(2 * np.pi * cycles[1] * t + 1.3)


def add_wrapped(out, event, start):
    idx = (start + np.arange(len(event))) % len(out)
    np.add.at(out, idx, event)


def chirp(f0, f1, seconds):
    t = np.arange(int(seconds * RATE)) / RATE
    phase = 2 * np.pi * (f0 * t + (f1 - f0) * t * t / (2 * seconds))
    return (np.sin(phase) + 0.3 * np.sin(2 * phase)) * np.sin(np.pi * t / seconds) ** 2


def write(name, signal, peak=0.7):
    os.makedirs(OUT, exist_ok=True)
    signal = signal / max(1e-9, np.abs(signal).max()) * peak
    path = os.path.join(OUT, f"{name}.wav")
    with wave.open(path, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes((signal * 32767).astype("<i2").tobytes())
    print(path)
    return signal


def forest(seconds=24.0):
    wind = shaped_noise(seconds, 150, 900)
    out = wind / np.abs(wind).max() * gusts(len(wind), (2, 5)) * 0.5
    for _ in range(14):  # bird calls: a few quick up/down chirps
        start = int(rng.uniform(0, seconds) * RATE)
        base = rng.uniform(2200, 4200)
        for k in range(int(rng.integers(2, 6))):
            note = chirp(base, base * rng.uniform(0.7, 1.4), rng.uniform(0.06, 0.14)) * rng.uniform(0.15, 0.3)
            add_wrapped(out, note, start + int(k * rng.uniform(0.1, 0.18) * RATE))
    return out


def river(seconds=12.0):
    water = shaped_noise(seconds, 300, 2500)
    out = water / np.abs(water).max() * gusts(len(water), (3, 7)) * 0.6
    for _ in range(int(seconds * 25)):  # bubbles: tiny rising blips
        f = rng.uniform(400, 1400)
        blip = chirp(f, f * 1.6, rng.uniform(0.02, 0.05)) * rng.uniform(0.03, 0.12)
        add_wrapped(out, blip, int(rng.uniform(0, seconds) * RATE))
    return out


if __name__ == "__main__":
    for name, signal in (("Amb_Forest", forest()), ("Amb_River", river())):
        written = write(name, signal)
        assert abs(written[0] - written[-1]) < 0.2, f"{name} does not loop cleanly"
