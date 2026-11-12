#!/usr/bin/env python3
"""CHIKENGUN 2.5 — генератор Data/Textures/BootSigil.png.

Берёт кибер-петуха (SVG-сигл) прямо из index.html веб-версии и рендерит
в PNG 1024x1024 с прозрачностью — чтобы нативный клиент Urho3D показывал
ровно тот же знак, что и веб-бут (паритет дизайна).

Зависимость:  npm install @resvg/resvg-js   (или resvg CLI)
Запуск:       python3 tools/gen_boot_sigil.py
"""
import os, pathlib, re, shutil, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC  = ROOT / "index.html"
OUT  = ROOT / "Data" / "Textures" / "BootSigil.png"
TMP  = ROOT / "build" / "sigil_svg"

def extract_svg() -> str:
    html = SRC.read_text(encoding="utf-8")
    i = html.index('<svg viewBox="0 0 240 240"')
    j = html.index("</svg>", i) + len("</svg>")
    svg = html[i:j]
    return svg.replace("<svg ", '<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" ', 1)

def main() -> int:
    TMP.mkdir(parents=True, exist_ok=True)
    svg_path = TMP / "sigil.svg"
    svg_path.write_text(extract_svg(), encoding="utf-8")

    # 1) python-путь через resvg-js (node)
    js = TMP / "render.js"
    # Модуль ищем в нескольких местах: у node резолв идёт от каталога скрипта,
    # поэтому @resvg/resvg-js передаём абсолютным путём.
    mod = None
    for base in (ROOT, TMP, pathlib.Path("/tmp/svgtool"), pathlib.Path.home()):
        cand = base / "node_modules" / "@resvg" / "resvg-js"
        if cand.is_dir():
            mod = cand
            break
    if mod is None:
        print("Нужен @resvg/resvg-js:  npm install @resvg/resvg-js", file=sys.stderr)
        return 1
    js.write_text(
        f"const {{Resvg}}=require({str(mod)!r});\n"
        "const fs=require('fs');\n"
        "const svg=fs.readFileSync(process.argv[2]);\n"
        "const r=new Resvg(svg,{fitTo:{mode:'width',value:1024},background:'rgba(0,0,0,0)'});\n"
        "fs.writeFileSync(process.argv[3], r.render().asPng());\n"
        "console.log('wrote', process.argv[3]);\n",
        encoding="utf-8")
    node = shutil.which("node")
    if node:
        try:
            subprocess.run([node, str(js), str(svg_path), str(OUT)], check=True)
            return 0
        except subprocess.CalledProcessError:
            pass
    # 2) resvg CLI
    if shutil.which("resvg"):
        subprocess.run(["resvg", "--width", "1024", str(svg_path), str(OUT)], check=True)
        return 0
    print("Нужен @resvg/resvg-js:  npm install @resvg/resvg-js", file=sys.stderr)
    return 1

if __name__ == "__main__":
    raise SystemExit(main())
