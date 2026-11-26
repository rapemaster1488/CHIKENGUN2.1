#!/usr/bin/env python3
# CHIKENGUN 2.5.5+ — генератор Data/Textures/SplashGlow.png (радиальное мягкое свечение для интро-надписей)
import numpy as np, os
from PIL import Image
SIZE = 512
c = (SIZE - 1) / 2.0
y, x = np.mgrid[0:SIZE, 0:SIZE]
d = np.sqrt((x - c) ** 2 + (y - c) ** 2) / c
a = np.clip(1.0 - d, 0.0, 1.0) ** 2.6
a = a * (0.55 + 0.45 * np.exp(-((d / 0.42) ** 2)))          # плотное ядро + мягкий ореол
a = np.clip(a * 255.0, 0, 255).astype(np.uint8)
rgba = np.stack([np.full_like(a, 255)] * 3 + [a], axis=-1)
out = os.path.join(os.path.dirname(__file__), "..", "Data", "Textures", "SplashGlow.png")
Image.fromarray(rgba, "RGBA").save(out)
print("wrote", os.path.normpath(out))
