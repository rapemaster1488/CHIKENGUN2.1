#!/usr/bin/env python3
"""Генерация куриных звуков для CHIKENGUN без внешних ассетов.

Пишет 16-битные PCM WAV (22050 Гц, моно) в bin/Data/Sounds и Data/Sounds.
Запуск:  python3 tools/gen_sounds.py
"""
import math
import os
import random
import struct
import wave

RATE = 22050


def write_wav(path, samples):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    peak = max(1e-6, max(abs(s) for s in samples))
    norm = [int(max(-1.0, min(1.0, s / peak * 0.85)) * 32767) for s in samples]
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", v) for v in norm))
    print("  %-42s %5.2f s" % (path, len(samples) / RATE))


def env(t, attack, decay):
    if t < attack:
        return t / attack
    return math.exp(-(t - attack) / decay)


def cluck(f0, f1, dur=0.16, noise=0.25):
    """Короткое «ко-ко»: пила с падающей частотой + щелчок в начале."""
    out = []
    n = int(RATE * dur)
    phase = 0.0
    for i in range(n):
        t = i / RATE
        f = f0 + (f1 - f0) * (t / dur)
        phase += 2 * math.pi * f / RATE
        saw = (phase / math.pi) % 2.0 - 1.0
        tone = 0.6 * saw + 0.4 * math.sin(phase)
        nz = (random.random() * 2.0 - 1.0) * noise * math.exp(-t / 0.02)
        out.append((tone + nz) * env(t, 0.004, dur * 0.35))
    return out


def squawk(dur=0.55):
    """Испуганный вопль: пила с падающей частотой, вибрато и хрипом."""
    out = []
    n = int(RATE * dur)
    phase = 0.0
    for i in range(n):
        t = i / RATE
        f = 900.0 * (1.0 - 0.65 * t / dur) + 60.0 * math.sin(2 * math.pi * 18.0 * t)
        phase += 2 * math.pi * f / RATE
        saw = (phase / math.pi) % 2.0 - 1.0
        nz = (random.random() * 2.0 - 1.0) * 0.18
        out.append((0.75 * saw + nz) * env(t, 0.01, dur * 0.5))
    return out


def splat(dur=0.28):
    """Яйцо всмятку: шумовой всплеск с быстрым затуханием."""
    out = []
    n = int(RATE * dur)
    low = 0.0
    for i in range(n):
        t = i / RATE
        nz = random.random() * 2.0 - 1.0
        low += 0.25 * (nz - low)          # простой low-pass
        body = 0.7 * low + 0.3 * nz
        out.append(body * env(t, 0.002, 0.05))
    return out


def gobble(dur=0.85):
    """Индюк-босс: низкое «гло-гло» с тремоло."""
    out = []
    n = int(RATE * dur)
    phase = 0.0
    for i in range(n):
        t = i / RATE
        f = 180.0 * (1.0 - 0.35 * t / dur) * (1.0 + 0.06 * math.sin(2 * math.pi * 22.0 * t))
        phase += 2 * math.pi * f / RATE
        saw = (phase / math.pi) % 2.0 - 1.0
        trem = 0.65 + 0.35 * math.sin(2 * math.pi * 9.0 * t)
        out.append(saw * trem * env(t, 0.02, dur * 0.55))
    return out


def sweep(dur=0.65):
    """Старт волны: восходящий свип с шумом."""
    out = []
    n = int(RATE * dur)
    phase = 0.0
    for i in range(n):
        t = i / RATE
        f = 220.0 * (1.0 + 2.0 * t / dur)
        phase += 2 * math.pi * f / RATE
        tone = 0.55 * math.sin(phase) + 0.25 * math.sin(2 * phase)
        nz = (random.random() * 2.0 - 1.0) * 0.08
        out.append((tone + nz) * env(t, 0.05, dur * 0.9))
    return out


SOUNDS = {
    "Chicken_Cluck.wav": cluck(560.0, 380.0),
    "Chicken_Cluck2.wav": cluck(720.0, 470.0, dur=0.13, noise=0.3),
    "Chicken_Squawk.wav": squawk(),
    "Chicken_Egg.wav": splat(),
    "Chicken_Boss.wav": gobble(),
    "Wave_Start.wav": sweep(),
}

if __name__ == "__main__":
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    random.seed(1337)
    for folder in ("bin/Data/Sounds", "Data/Sounds"):
        print(folder)
        for name, data in SOUNDS.items():
            write_wav(os.path.join(root, folder, name), data)
    print("готово")
