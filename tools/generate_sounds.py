#!/usr/bin/env python3
"""Synthesises the placeholder sound effects in assets/audio/.

Run from the repository root: python3 tools/generate_sounds.py
Deterministic (fixed seed), so re-running gives byte-identical files.
"""
import math
import random
import struct
import wave
from pathlib import Path

RATE = 22050
OUT = Path(__file__).resolve().parent.parent / "assets" / "audio"


def write(name, samples):
    OUT.mkdir(parents=True, exist_ok=True)
    with wave.open(str(OUT / name), "wb") as file:
        file.setnchannels(1)
        file.setsampwidth(2)
        file.setframerate(RATE)
        file.writeframes(b"".join(
            struct.pack("<h", int(max(-1.0, min(1.0, s)) * 32767)) for s in samples))


def shot():
    # Short square-wave chirp sweeping down from 1400 Hz to 300 Hz.
    length = int(RATE * 0.12)
    phase = 0.0
    for i in range(length):
        t = i / length
        phase += (1400 - 1100 * t) / RATE
        yield (0.35 if phase % 1.0 < 0.5 else -0.35) * (1.0 - t) ** 2


def explosion(rng):
    # Low-passed noise with an exponential decay.
    length = int(RATE * 0.6)
    low = 0.0
    for i in range(length):
        t = i / length
        low += (rng.uniform(-1, 1) - low) * (0.25 - 0.2 * t)
        yield low * 1.6 * math.exp(-4.5 * t)


def player_hit(rng):
    # Falling tone mixed with noise.
    length = int(RATE * 0.45)
    phase = 0.0
    for i in range(length):
        t = i / length
        phase += (520 - 400 * t) / RATE
        tone = math.sin(2 * math.pi * phase)
        yield (0.5 * tone + 0.25 * rng.uniform(-1, 1)) * (1.0 - t)


if __name__ == "__main__":
    rng = random.Random(42)
    write("shot.wav", list(shot()))
    write("explosion.wav", list(explosion(rng)))
    write("player_hit.wav", list(player_hit(rng)))
    print(f"sounds written to {OUT}")
