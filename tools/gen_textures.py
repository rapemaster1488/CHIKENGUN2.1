#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Генератор HD-текстур для CHIKENGUN 2.1 (PBR-ready: diffuse + normal).
Использует только stdlib+math, пишет BMP (Urho3D читает BMP без zlib).
Запуск:  python3 tools/gen_textures.py bin/Data/TexturesHD
"""
import math, os, random, struct, sys

random.seed(1337)
S = 512  # размер текстур

def clamp(v): return 0 if v < 0 else (255 if v > 255 else int(v))

def save_bmp(path, pixels):
    """pixels: list of rows, each row list of (r,g,b) bottom-up BMP."""
    w, h = len(pixels[0]), len(pixels)
    pad = (4 - (w * 3) % 4) % 4
    rowsize = w * 3 + pad
    data = bytearray()
    for y in range(h - 1, -1, -1):
        row = bytearray()
        for (r, g, b) in pixels[y]:
            row += bytes((clamp(b), clamp(g), clamp(r)))
        row += b'\x00' * pad
        data += row
    filesize = 54 + len(data)
    header = struct.pack('<2sIHHI', b'BM', filesize, 0, 0, 54)
    dib = struct.pack('<IiiHHIIiiII', 40, w, h, 1, 24, 0, len(data), 2835, 2835, 0, 0)
    with open(path, 'wb') as f:
        f.write(header + dib + data)

def height_to_normal(hf, strength=2.0):
    """hf[y][x] float 0..1 -> normal map pixels."""
    n = len(hf)
    out = []
    for y in range(n):
        row = []
        for x in range(n):
            l = hf[y][(x - 1) % n]; r = hf[y][(x + 1) % n]
            u = hf[(y - 1) % n][x]; d = hf[(y + 1) % n][x]
            nx = (l - r) * strength; ny = (u - d) * strength; nz = 1.0
            ln = math.sqrt(nx*nx + ny*ny + nz*nz)
            nx /= ln; ny /= ln; nz /= ln
            row.append((int((nx*0.5+0.5)*255), int((ny*0.5+0.5)*255), int((nz*0.5+0.5)*255)))
        out.append(row)
    return out

def noise_grid(cells, octaves=4):
    """Multi-octave value noise tiled on SxS."""
    field = [[0.0]*S for _ in range(S)]
    amp = 1.0; tot = 0.0
    for o in range(octaves):
        c = cells * (2 ** o)
        rnd = [[random.random() for _ in range(c)] for _ in range(c)]
        sx = S / c
        for y in range(S):
            gy = y / sx; iy = int(gy) % c; fy = gy - int(gy)
            fy = fy*fy*(3-2*fy)
            for x in range(S):
                gx = x / sx; ix = int(gx) % c; fx = gx - int(gx)
                fx = fx*fx*(3-2*fx)
                v00 = rnd[iy][ix]; v10 = rnd[iy][(ix+1) % c]
                v01 = rnd[(iy+1) % c][ix]; v11 = rnd[(iy+1) % c][(ix+1) % c]
                a = v00+(v10-v00)*fx; b = v01+(v11-v01)*fx
                field[y][x] += amp*(a+(b-a)*fy)
        tot += amp; amp *= 0.5
    for y in range(S):
        for x in range(S):
            field[y][x] /= tot
    return field

def stone():
    n1 = noise_grid(8, 5); n2 = noise_grid(32, 3)
    hf = [[0.0]*S for _ in range(S)]; px = []
    for y in range(S):
        row = []
        for x in range(S):
            v = n1[y][x]*0.7 + n2[y][x]*0.3
            # трещины
            crack = abs(n1[y][x]-0.5)
            dark = 1.0 if crack < 0.02 else 0.96
            g = (108 + v*70) * dark
            tint_r = g*1.03; tint_b = g*0.97
            row.append((tint_r, g, tint_b))
            hf[y][x] = v*0.8 + (0.3 if crack < 0.02 else 0.5)
        px.append(row)
    return px, height_to_normal(hf, 2.4)

def metal():
    brush = [[0.0]*S for _ in range(S)]; px = []
    for y in range(S):
        streak = random.random()
        row = []
        for x in range(S):
            v = 0.55 + 0.10*math.sin(x*0.35 + streak*20) + random.uniform(-0.03, 0.03)
            v += 0.06*noise_grid(4,2)[y][x] if False else 0
            g = 90 + v*120
            row.append((g*0.98, g*0.99, g*1.05))
            brush[y][x] = v
        px.append(row)
    return px, height_to_normal(brush, 0.6)

def tech_panel():
    px = []; hf = [[0.0]*S for _ in range(S)]
    panel = 128
    n = noise_grid(16, 3)
    for y in range(S):
        row = []
        for x in range(S):
            py, pxx = y % panel, x % panel
            border = py < 4 or pxx < 4 or py > panel-5 or pxx > panel-5
            seam = (y // panel + x // panel) % 2 == 0
            base = 52 if seam else 62
            if border:
                c = (20, 22, 26); hgt = 0.2
            else:
                v = base + n[y][x]*26
                c = (v*0.85, v*0.95, v*1.15)  # холодный синий оттенок
                hgt = 0.6 + n[y][x]*0.3
                # заклёпки по углам панелей
                for rx, ry in ((18,18),(panel-18,18),(18,panel-18),(panel-18,panel-18)):
                    if (pxx-rx)**2 + (py-ry)**2 < 36:
                        c = (v*1.2+30, v*1.2+32, v*1.2+36); hgt = 0.9
            row.append(c); hf[y][x] = hgt
        px.append(row)
    return px, height_to_normal(hf, 2.0)

def crate_wood():
    px = []; hf = [[0.0]*S for _ in range(S)]
    plank = 85
    for y in range(S):
        for x in range(S):
            py = y % plank
            gap = py < 3 or py > plank-4
            grain = 0.5 + 0.5*math.sin(x*0.12 + math.sin(y*0.05 + x*0.02)*3)
            if gap:
                c = (35, 24, 14); h = 0.1
            else:
                tone = 118 + grain*46 + random.uniform(-6, 6)
                c = (tone, tone*0.72, tone*0.44); h = 0.5 + grain*0.4
            px_row = None
            hf[y][x] = h
            if not hasattr(crate_wood, '_rows'): pass
            # collect per-row below
    # rebuild rows properly
    px = []
    for y in range(S):
        row = []
        for x in range(S):
            py = y % plank
            gap = py < 3 or py > plank-4
            if gap:
                row.append((35, 24, 14))
            else:
                grain = 0.5 + 0.5*math.sin(x*0.12 + math.sin((y//plank)*7 + y*0.05 + x*0.02)*3)
                tone = 118 + grain*46
                row.append((tone, tone*0.72, tone*0.44))
        px.append(row)
    # металлические уголки
    for y in range(S):
        for x in range(S):
            edge = x < 14 or x > S-15 or y < 14 or y > S-15
            if edge:
                px[y][x] = (70, 70, 74)
                hf[y][x] = 0.95
    return px, height_to_normal(hf, 1.6)

def checker_floor():
    px = []; hf = [[0.0]*S for _ in range(S)]
    tile = 128
    n = noise_grid(24, 3)
    for y in range(S):
        row = []
        for x in range(S):
            ty, tx = y // tile, x // tile
            light = (ty + tx) % 2 == 0
            grout = (y % tile) < 5 or (x % tile) < 5
            if grout:
                c = (44, 44, 48); h = 0.15
            elif light:
                v = 175 + n[y][x]*40
                c = (v, v*0.99, v*0.92); h = 0.6 + n[y][x]*0.25
            else:
                v = 58 + n[y][x]*22
                c = (v, v*0.98, v*1.06); h = 0.6 + n[y][x]*0.25
            row.append(c); hf[y][x] = h
        px.append(row)
    return px, height_to_normal(hf, 1.8)

def sky_day():
    """Вертикальный градиент небо->горизонт + облака (не тайлится, просто фон)."""
    n = noise_grid(6, 5)
    px = []
    for y in range(S):
        t = y / S
        r = 150 - t*95; g = 190 - t*100; b = 235 - t*60
        row = []
        for x in range(S):
            cloud = max(0.0, n[y][x] - 0.55) * 3.0
            row.append((r + cloud*105, g + cloud*100, b + cloud*80))
        px.append(row)
    return px

def main():
    out = sys.argv[1] if len(sys.argv) > 1 else 'bin/Data/TexturesHD'
    os.makedirs(out, exist_ok=True)
    jobs = {
        'StoneHD': stone(),
        'MetalPanel': tech_panel(),
        'FloorTile': checker_floor(),
        'CrateWood': crate_wood(),
        'BrushedMetal': metal(),
    }
    for name, (diff, norm) in jobs.items():
        save_bmp(os.path.join(out, name + 'Diffuse.bmp'), diff)
        save_bmp(os.path.join(out, name + 'Normal.bmp'), norm)
        print('OK', name)
    save_bmp(os.path.join(out, 'SkyDay.bmp'), sky_day())
    print('Done ->', out)

if __name__ == '__main__':
    main()
