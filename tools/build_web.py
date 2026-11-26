#!/usr/bin/env python3
"""CHIKENGUN — офлайн сборка веб-арены в один файл (запуск на ПК без сервера).

Что делает:
  1. Инлайнит движок web/webgl2_arena.js прямо в HTML.
  2. Упаковывает все PBR-текстуры, Sky4K-куб и арт загрузки в data: URL
     (window.__CG_ASSETS), поэтому file:// не упирается в CORS.
  3. Встраивает banya.html / minigames.html / casino.html через srcdoc —
     вкладки работают двойным кликом по файлу, без python-сервера.
  4. Звуки/музыку оставляет рядом: при их отсутствии движок просто молчит,
     зато итоговый файл остаётся компактным.

Использование:
    python3 tools/build_web.py                 # -> CHIKENGUN_OFFLINE.html
    python3 tools/build_web.py --sounds        # + звуки/музыка в data: URL
    python3 tools/build_web.py --out /path/CHIKENGUN.html
    python3 tools/build_web.py --check         # только проверить размеры

Результат — один самодостаточный .html: двойной клик, работает офлайн.
"""
from __future__ import annotations

import argparse
import base64
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# --- files referenced by index.html / webgl2_arena.js -------------------------
TEXTURE_GLOBS = [
    "Data/Textures/Sky4K_PosX.png", "Data/Textures/Sky4K_NegX.png",
    "Data/Textures/Sky4K_PosY.png", "Data/Textures/Sky4K_NegY.png",
    "Data/Textures/Sky4K_PosZ.png", "Data/Textures/Sky4K_NegZ.png",
    "Data/Textures/LoadingBg4K.png",
]
MATERIAL_PREFIXES = [
    "FloorTile", "StoneHD", "MetalPanel", "BrushedMetal",
    "CrateWood", "FeatherPBR", "HazardMetal",
]
MATERIAL_SUFFIXES = ["Diffuse", "Normal", "RoughMetal"]

MIME = {".png": "image/png", ".jpg": "image/jpeg", ".jpeg": "image/jpeg",
        ".wav": "audio/wav", ".ogg": "audio/ogg", ".mp3": "audio/mpeg"}


def read(path: str) -> str:
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


def data_url(path: str) -> str:
    ext = os.path.splitext(path)[1].lower()
    mime = MIME.get(ext, "application/octet-stream")
    with open(path, "rb") as f:
        payload = base64.b64encode(f.read()).decode("ascii")
    return f"data:{mime};base64,{payload}"


def collect_assets(with_sounds: bool) -> dict[str, str]:
    """repo-relative path -> data: URL"""
    files: list[str] = []
    for rel in TEXTURE_GLOBS:
        p = os.path.join(ROOT, rel)
        if os.path.isfile(p):
            files.append(rel)
    for prefix in MATERIAL_PREFIXES:
        for suffix in MATERIAL_SUFFIXES:
            rel = f"Data/Textures/{prefix}{suffix}.png"
            if os.path.isfile(os.path.join(ROOT, rel)):
                files.append(rel)
    if with_sounds:
        for folder in ("Data/Sounds", "Data/Music"):
            d = os.path.join(ROOT, folder)
            if not os.path.isdir(d):
                continue
            for name in sorted(os.listdir(d)):
                if os.path.splitext(name)[1].lower() in (".wav", ".ogg", ".mp3"):
                    files.append(f"{folder}/{name}")
    out = {}
    for rel in dict.fromkeys(files):
        out[rel] = data_url(os.path.join(ROOT, rel))
    return out


def js_escape_url(u: str) -> str:
    # data: URLs contain no characters that need escaping in a JS string literal
    # beyond backslashes/quotes, which base64 never produces.
    return u


def inline_subpages(html: str) -> str:
    """Убрать внешние страницы из разметки: iframe -> srcdoc, ссылки -> inline."""

    def esc_attr(v: str) -> str:
        return (v.replace("&", "&amp;").replace('"', "&quot;")
                 .replace("<", "&lt;").replace(">", "&gt;"))

    def iframe_sub(m: re.Match) -> str:
        tag = m.group(0)
        src_m = re.search(r'data-src="([^"?]+)', tag)
        if not src_m:
            return tag
        page_path = os.path.join(ROOT, src_m.group(1))
        if not os.path.isfile(page_path):
            return tag
        tag = re.sub(r'\sdata-src="[^"]*"', "", tag)
        # attribute must go INSIDE the tag, right before the closing '>'
        tag = tag.rstrip()
        assert tag.endswith(">")
        return tag[:-1].rstrip() + f' data-srcdoc="{esc_attr(read(page_path))}">'

    html = re.sub(r"<iframe\b[^>]*>", iframe_sub, html)
    for name in ("minigames.html", "casino.html"):
        html = html.replace(
            f'<a class="tab-btn ext" href="{name}" data-offline="link">',
            f'<a class="tab-btn ext" href="#" data-inline-page="{name}" data-offline="link">',
        )
    return html


def asset_url_or_data(rel: str, assets: dict[str, str]) -> str:
    """Готовый data: URL для ассета (докидывает его в кэш при необходимости)."""
    if rel not in assets:
        assets[rel] = data_url(os.path.join(ROOT, rel))
    return assets[rel]


def js_literal(value: str) -> str:
    """Безопасный JS-строковый литерал (JSON-экранирование + < защита от </script>)."""
    return json.dumps(value, ensure_ascii=False).replace("</", "<\\/")


def build(out_path: str, with_sounds: bool) -> None:
    html = read(os.path.join(ROOT, "index.html"))
    engine = read(os.path.join(ROOT, "web", "webgl2_arena.js"))
    assets = collect_assets(with_sounds)

    # ---- 1. встроить движок вместо внешнего <script src> ----
    engine_tag = re.search(r'<script src="web/webgl2_arena\.js[^"]*"[^>]*></script>', html)
    if not engine_tag:
        sys.exit("[build] не найден тег движка в index.html")
    engine_block = "<script>\n/* CHIKENGUN engine — inlined by tools/build_web.py */\n" + engine + "\n</script>"
    html = html.replace(engine_tag.group(0), engine_block)

    # ---- 2. карта ассетов (data: URL) перед движком ----
    lines = ["<script>", "/* Offline assets: текстуры/Sky4K встроены как data: URL */",
             "window.__CG_ASSETS = {"]
    for rel, url in assets.items():
        lines.append("  " + json.dumps(rel) + ": " + json.dumps(url) + ",")
    lines += ["};", "</script>"]
    html = html.replace(engine_block, "\n".join(lines) + "\n" + engine_block, 1)

    # ---- 2b. CSS url(...) -> data: URL (загрузочный арт и прочие картинки) ----
    def css_url(m: re.Match) -> str:
        rel = m.group(1)
        local = os.path.join(ROOT, rel)
        if not os.path.isfile(local):
            return m.group(0)
        return 'url("' + asset_url_or_data(rel, assets) + '")'

    html = re.sub(r'url\("((?:Data|web)/[^"]+)"\)', css_url, html)

    # ---- 3. вложенные страницы ----
    html = inline_subpages(html)

    # ---- 4. тела arcade/casino для ссылок ----
    pages = {}
    for name in ("minigames.html", "casino.html"):
        p = os.path.join(ROOT, name)
        if os.path.isfile(p):
            pages[name] = read(p)
    pages_block = ("<script>\n/* Встроенные страницы для офлайн-ссылок */\nwindow.__CG_PAGES = {"
                   + ",".join("  " + json.dumps(k) + ": " + js_literal(v) for k, v in pages.items())
                   + "};\n</script>")

    hook = """
<script>
/* Offline (file://): оживляем srcdoc-фреймы и внутренние страницы */
(() => {
  const hydrate = () => {
    document.querySelectorAll("iframe[data-srcdoc]").forEach(f => {
      if (!f.getAttribute("srcdoc")) f.setAttribute("srcdoc", f.getAttribute("data-srcdoc"));
      f.removeAttribute("data-srcdoc");
    });
    document.querySelectorAll("a[data-inline-page]").forEach(a => {
      a.addEventListener("click", ev => {
        ev.preventDefault();
        const key = a.getAttribute("data-inline-page");
        const page = (window.__CG_PAGES || {})[key];
        if (!page) return;
        const w = window.open("", "_blank");
        if (w) { w.document.open(); w.document.write(page); w.document.close(); }
      });
    });
  };
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", hydrate);
  else hydrate();
})();
</script>
"""
    html = html.replace("</body>", pages_block + hook + "</body>")

    with open(out_path, "w", encoding="utf-8") as f:
        f.write(html)

    size = os.path.getsize(out_path)
    print(f"[build] ассетов встроено: {len(assets)}")
    print(f"[build] {out_path}  {size/1024/1024:.2f} MB")
    print("[build] Запуск: двойной клик по файлу (без сервера и без интернета)")


def main() -> None:
    ap = argparse.ArgumentParser(description="Офлайн-сборка веб-арены CHIKENGUN в один HTML")
    ap.add_argument("--out", default=os.path.join(ROOT, "CHIKENGUN_OFFLINE.html"))
    ap.add_argument("--sounds", action="store_true", help="встроить WAV/OGG (файл станет ~+3.3 МБ)")
    ap.add_argument("--check", action="store_true", help="только посчитать размеры ассетов")
    args = ap.parse_args()

    assets = collect_assets(args.sounds)
    total = sum(len(v) for v in assets.values())
    if args.check:
        print(f"[check] {len(assets)} ассетов, ~{total/1024/1024:.2f} MB в base64")
        missing = [r for r in TEXTURE_GLOBS if not os.path.isfile(os.path.join(ROOT, r))]
        print("[check] отсутствуют:", missing or "нет")
        return
    build(args.out, args.sounds)


if __name__ == "__main__":
    main()
