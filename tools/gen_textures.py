#!/usr/bin/env python3
"""
Генератор 4K PBR текстур (4096x4096) для CHIKENGUN 2.1 — 4K ULTRA EDITION.
Автоматически компилирует и запускает быстрый многопоточный C++17 генератор
tools/gen_textures_4k.cpp (-O3 -pthread), создавая полный набор:
  - Diffuse + Tangent Normal + RoughnessMetallic карты 4096x4096
  - 6 бесшовных граней 360° Skybox4K Cubemap (IBL + отражения)
  - 4K UHD (3840x2160) кибер-арт загрузочного экрана LoadingBg4K.png
"""
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CPP_SRC = os.path.join(ROOT, "tools", "gen_textures_4k.cpp")


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build", "Data", "Textures")
    size = int(sys.argv[2]) if len(sys.argv) > 2 else 4096
    os.makedirs(out_dir, exist_ok=True)

    cxx = shutil.which("g++") or shutil.which("clang++") or shutil.which("c++")
    if cxx and os.path.isfile(CPP_SRC):
        bin_dir = os.path.join(ROOT, "build")
        os.makedirs(bin_dir, exist_ok=True)
        bin_path = os.path.join(bin_dir, "gen_textures_4k")
        if (not os.path.isfile(bin_path)) or (os.path.getmtime(CPP_SRC) > os.path.getmtime(bin_path)):
            print(f"[gen_textures] Компиляция C++17 4K-генератора ({cxx} -O3 -pthread)...")
            subprocess.check_call([cxx, "-O3", "-std=c++17", "-pthread", CPP_SRC, "-o", bin_path])
        subprocess.check_call([bin_path, out_dir, str(size)])
        return

    print("[gen_textures] Внимание: C++ компилятор не найден; установите g++ для генерации 4K текстур.", file=sys.stderr)
    sys.exit(1)


if __name__ == "__main__":
    main()
