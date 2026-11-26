#!/usr/bin/env python3
"""Generate a small deterministic RGBA UI atlas; standard library only."""
import math
import pathlib
import struct
import zlib

SIZE = 64
COLORS = [(65, 128, 164), (162, 58, 65), (65, 143, 106),
          (97, 76, 150), (174, 126, 47), (49, 57, 66)]

def generate():
    rows = []
    for y in range(SIZE):
        row = bytearray()
        for x in range(SIZE * len(COLORS)):
            dx, dy = (x % SIZE - 31.5), (y - 31.5)
            r = math.hypot(dx, dy)
            angle = math.atan2(dy, dx)
            base = COLORS[x // SIZE]
            alpha = round(255 * max(0, min(1, 30.5-r)))
            if r > 28 or 21 < r < 23:
                rgb = (222, 211, 182)
            elif r > 23 and int((angle + math.pi) * 6 / math.pi) % 2 == 0:
                rgb = (236, 232, 214)
            elif r < 20:
                rgb = (29, 37, 39)
            else:
                rgb = base
            shade = 1.0 - dy * 0.002 + 0.025 * math.sin(dx * 2 + dy)
            row.extend([*(max(0, min(255, round(c * shade))) for c in rgb), alpha])
        rows.append(b'\0' + row)
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag+data))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', SIZE*6, SIZE, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(b''.join(rows), 9)) + chunk(b'IEND', b''))

if __name__ == '__main__':
    path = pathlib.Path(__file__).resolve().parents[1] / 'Data/Textures/UI/CasinoChips.png'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(generate())
