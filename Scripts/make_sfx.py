"""Synthesises the placeholder sound effects (M5) into SourceArt/Audio as 16-bit mono WAVs. Standard-library Python
only; run with any Python 3 (`python Scripts/make_sfx.py`), then Scripts/setup_m5.py imports them as
/Game/Shinobi/Audio/SFX_*. Deterministic (fixed seed). Swap any file for a real recording with the same name."""
import math
import os
import random
import struct
import wave

RATE = 44100
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "SourceArt", "Audio")
rng = random.Random(42)


def samples(seconds):
    return range(int(seconds * RATE))


def noise():
    return rng.uniform(-1.0, 1.0)


def lowpass(signal, cutoff_at):
    """One-pole low-pass whose cutoff (Hz) may vary over time: cutoff_at(t)."""
    out, y = [], 0.0
    for i, x in enumerate(signal):
        a = 1.0 - math.exp(-2.0 * math.pi * cutoff_at(i / RATE) / RATE)
        y += a * (x - y)
        out.append(y)
    return out


def envelope(t, attack, length):
    if t < attack:
        return t / attack
    return max(0.0, 1.0 - (t - attack) / max(length - attack, 1e-6))


def normalise(signal, peak=0.8):
    top = max(1e-9, max(abs(x) for x in signal))
    return [x / top * peak for x in signal]


def write(name, signal):
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, f"SFX_{name}.wav")
    with wave.open(path, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, x)) * 32767)) for x in normalise(signal)))
    print(path)


def whoosh(length, low, high):
    raw = [noise() for _ in samples(length)]
    filtered = lowpass(raw, lambda t: low + (high - low) * math.sin(math.pi * t / length))
    return [x * math.sin(math.pi * i / RATE / length) ** 2 for i, x in enumerate(filtered)]


def tone(length, freqs, decay):
    return [sum(math.sin(2 * math.pi * f * i / RATE) * a for f, a in freqs) * math.exp(-decay * i / RATE) for i in samples(length)]


write("Whoosh", whoosh(0.25, 300, 2500))
write("Swish", whoosh(0.2, 800, 5000))
write("Impact", [math.sin(2 * math.pi * (140 - 80 * i / RATE / 0.18) * i / RATE) * math.exp(-18 * i / RATE)
                 + noise() * math.exp(-60 * i / RATE) * 0.6 for i in samples(0.18)])
explosion = lowpass([noise() for _ in samples(1.0)], lambda t: 3000 * math.exp(-3 * t) + 150)
write("Explosion", [x * envelope(i / RATE, 0.01, 1.0) ** 2 + math.sin(2 * math.pi * 55 * i / RATE) * math.exp(-4 * i / RATE) * 0.5
                    for i, x in enumerate(explosion)])
zap, burst = [], 0.0
for i in samples(0.6):
    if rng.random() < 0.004:
        burst = rng.uniform(0.5, 1.0)
    burst *= 0.9992
    zap.append((1.0 if math.sin(2 * math.pi * rng.choice((900, 1300, 1700)) * i / RATE) > 0 else -1.0) * burst * noise() * envelope(i / RATE, 0.02, 0.6))
write("Zap", zap)
write("Whirr", [(math.sin(2 * math.pi * (220 + 400 * i / RATE) * i / RATE) * (0.6 + 0.4 * math.sin(2 * math.pi * 18 * i / RATE)) + noise() * 0.15)
                * envelope(i / RATE, 0.05, 0.8) for i in samples(0.8)])
write("Poof", [x * math.exp(-9 * i / RATE) for i, x in enumerate(lowpass([noise() for _ in samples(0.4)], lambda t: 1200))])
write("Cast", tone(0.5, [(880, 0.6), (1320, 0.4), (1760, 0.2)], 7))
write("Gong", tone(2.0, [(110, 0.7), (262, 0.35), (431, 0.25), (587, 0.15)], 1.8))
victory = []
for n, f in enumerate((523, 659, 784, 1047)):
    victory += tone(0.18 if n < 3 else 0.9, [(f, 0.7), (f * 2, 0.2)], 4)
write("Victory", victory)
write("Thud", [math.sin(2 * math.pi * 70 * i / RATE) * math.exp(-14 * i / RATE) + noise() * math.exp(-40 * i / RATE) * 0.3 for i in samples(0.3)])
