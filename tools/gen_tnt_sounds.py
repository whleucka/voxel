#!/usr/bin/env python3
"""Make TNT's sounds: a few explosion booms and a fizzing fuse.

Writes assets/sound-effects/tnt/boom_000.ogg .. boom_002.ogg and fuse_000.ogg.
Every sound comes from a fixed seed, so running this again makes the same
files; change the numbers below and run it to tweak them. Nothing recorded,
so there's no licence to credit.

Requires numpy and ffmpeg (for the Ogg Vorbis encoding).
Usage: python3 tools/gen_tnt_sounds.py
"""

import subprocess
import tempfile
import wave
from pathlib import Path

import numpy as np

RATE = 44100
OUT = Path(__file__).resolve().parent.parent / "assets" / "sound-effects" / "tnt"


def lowpass(x, cutoff):
    """One-pole low-pass filter at `cutoff` Hz (a gentle slope)."""
    a = np.exp(-2.0 * np.pi * cutoff / RATE)
    y = np.empty_like(x)
    acc = 0.0
    for i, v in enumerate(x):
        acc = (1.0 - a) * v + a * acc
        y[i] = acc
    return y


def sweep_lowpass(x, start, end):
    """A low-pass whose cutoff falls from `start` to `end` Hz over the sound,
    so the blast's crack dulls into a rumble."""
    cut = np.geomspace(start, end, len(x))
    a = np.exp(-2.0 * np.pi * cut / RATE)
    y = np.empty_like(x)
    acc = 0.0
    for i, v in enumerate(x):
        acc = (1.0 - a[i]) * v + a[i] * acc
        y[i] = acc
    return y


def normalise(x, peak=0.9):
    return x * (peak / max(1e-9, np.max(np.abs(x))))


def boom(seed):
    rng = np.random.default_rng(seed)
    seconds = 2.2
    n = int(RATE * seconds)
    t = np.arange(n) / RATE
    # The blast: noise that cracks bright, then dulls and rolls away.
    noise = rng.standard_normal(n)
    body = noise
    for _ in range(3):  # stacked, for a steep slope: a boom, not a hiss
        body = sweep_lowpass(body, 1800.0 + 300.0 * seed, 90.0)
    body *= np.exp(-t * (2.6 + 0.3 * seed))
    # The thump under it: a low tone that drops in pitch.
    freq = 38.0 + 30.0 * np.exp(-t * 6.0) + 4.0 * seed
    thump = np.sin(2.0 * np.pi * np.cumsum(freq) / RATE) * np.exp(-t * 4.0)
    # Debris pattering down after.
    patter = lowpass(lowpass(rng.standard_normal(n) * (rng.random(n) < 0.002), 1500.0), 1500.0)
    patter *= np.clip((t - 0.3) * 3.0, 0.0, 1.0) * np.exp(-t * 1.8) * 30.0
    attack = np.clip(t / 0.004, 0.0, 1.0)  # no click at the very start
    body = normalise(body)
    return normalise((body * 1.0 + thump * 0.8 + patter * 0.25) * attack)


def fuse():
    rng = np.random.default_rng(99)
    seconds = 4.0
    n = int(RATE * seconds)
    t = np.arange(n) / RATE
    # A steady hiss (high-passed noise)...
    noise = rng.standard_normal(n)
    hiss = noise - lowpass(noise, 1800.0)
    hiss *= 0.35 + 0.15 * np.sin(2.0 * np.pi * 7.0 * t + rng.random() * 6.0)
    # ...with sparks crackling in it.
    sparks = (rng.random(n) < 0.0015) * rng.uniform(0.5, 1.0, n)
    crackle = lowpass(sparks, 5000.0) * 9.0
    envelope = np.clip(t / 0.05, 0.0, 1.0) * np.clip((seconds - t) / 0.2, 0.0, 1.0)
    return normalise((hiss + crackle) * envelope, 0.8)


def write_ogg(samples, path):
    pcm = (np.clip(samples, -1.0, 1.0) * 32767).astype("<i2")
    with tempfile.TemporaryDirectory() as tmp:
        wav_path = Path(tmp) / "out.wav"
        with wave.open(str(wav_path), "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(RATE)
            w.writeframes(pcm.tobytes())
        subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", str(wav_path),
                        "-c:a", "libvorbis", "-q:a", "4", str(path)], check=True)
    print(f"wrote {path}")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for i in range(3):
        write_ogg(boom(i), OUT / f"boom_{i:03d}.ogg")
    write_ogg(fuse(), OUT / "fuse_000.ogg")


if __name__ == "__main__":
    main()
