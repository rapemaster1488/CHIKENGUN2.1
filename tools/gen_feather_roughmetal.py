#!/usr/bin/env python3
"""
CHIKENGUN 2.1 — генератор FeatherPBRRoughMetal.png (512x512, тайловый).

Изначальный файл был плейсхолдером (сплошное пурпурное полотно), из-за чего
PBR-шейдер читал из него мусор. Этот скрипт рисует корректную Rough/Metal карту
в конвенции движка (как читает нативный Urho3D и WebGL2-шейдер 2.4+):

  R = roughness ≈ 0.62..0.78 с мягкими перьями-streak'ами и value-noise
  G = 0    (не металл)
  B = 255  (AO = 1, «без occlusion»)

Тайлит по обеим осям (бесшовно), stdlib-only (zlib + struct → PNG).

Запуск:  python3 tools/gen_feather_roughmetal.py [выходной.png ...]
"""
import struct
import sys
import zlib

S = 512            # размер текстуры
PERIOD = 32        # период value-noise решётки (делитель S → тайлится)
OCTAVES = [(32, 0.55, 7), (16, 0.30, 19), (8, 0.15, 42)]  # (ячейки, вес, seed)


def hash2(x, y, period, seed):
    x %= period
    y %= period
    h = (x * 374761393 + y * 668265263 + seed * 1442695041) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFFFFFF) / 0xFFFFFFFF


def value_noise(u, v, period, seed):
    """u,v ∈ [0,1); билинейная интерполяция сглаженного value-noise на торе."""
    x, y = u * period, v * period
    x0, y0 = int(x), int(y)
    fx, fy = x - x0, y - y0
    sx = fx * fx * (3.0 - 2.0 * fx)
    sy = fy * fy * (3.0 - 2.0 * fy)
    n00 = hash2(x0, y0, period, seed)
    n10 = hash2(x0 + 1, y0, period, seed)
    n01 = hash2(x0, y0 + 1, period, seed)
    n11 = hash2(x0 + 1, y0 + 1, period, seed)
    nx0 = n00 + (n10 - n00) * sx
    nx1 = n01 + (n11 - n01) * sx
    return nx0 + (nx1 - nx0) * sy


def roughness_at(u, v):
    base = 0.0
    total = 0.0
    for period, weight, seed in OCTAVES:
        base += value_noise(u, v, period, seed) * weight
        total += weight
    n = base / total                                   # 0..1
    # Вертикальные «перьевые» бороздки: лёгкая синусоидальная рябь по x
    streak = 0.5 + 0.5 * __import__("math").sin((u * 64.0 + n * 3.0) * 6.2831853)
    r = 0.60 + n * 0.14 + streak * 0.055               # ≈ 0.60..0.795
    return max(0.05, min(0.98, r))


def build_rows():
    rows = []
    for j in range(S):
        v = j / S
        row = bytearray(1 + S * 3)                     # filter byte + RGB
        row[0] = 0
        for i in range(S):
            u = i / S
            r = int(roughness_at(u, v) * 255.0 + 0.5)
            o = 1 + i * 3
            row[o] = r                                 # R: roughness (Urho3D packing)
            row[o + 1] = 0                             # G: metal = 0
            row[o + 2] = 255                           # B: AO = 1 («без occlusion»)
        rows.append(bytes(row))
    return b"".join(rows)


def write_png(path, raw_scanlines):
    def chunk(tag, payload):
        c = struct.pack(">I", len(payload)) + tag + payload
        c += struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)
        return c

    ihdr = struct.pack(">IIBBBBB", S, S, 8, 2, 0, 0, 0)  # 8-bit truecolor
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", ihdr)
           + chunk(b"IDAT", zlib.compress(raw_scanlines, 9))
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


def main():
    targets = sys.argv[1:] or ["Data/Textures/FeatherPBRRoughMetal.png"]
    raw = build_rows()
    for t in targets:
        write_png(t, raw)
        print(f"[feather-rm] записано: {t} ({S}x{S})")


if __name__ == "__main__":
    main()
