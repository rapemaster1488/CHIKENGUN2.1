#!/usr/bin/env python3
"""Автотесты офлайн-сборки веб-арены (tools/build_web.py).

    python3 tests/web/test_offline_build.py

Проверяет именно те места, где сборщик уже ломался:
  * атрибут data-srcdoc должен оказаться ВНУТРИ тега <iframe>, а не после '>';
  * встроенные страницы (banya/minigames/casino) не должны рвать JS-литералы;
  * все инлайн-<script> блоки обязаны быть синтаксически валидными (проверка через node);
  * карта ассетов содержит только data: URL и все ключи на месте;
  * extern-ссылки переписаны, движок встроен, внешних <script src> на локальные файлы нет.
Без внешних зависимостей: только стандартная библиотека Python + node (есть в CI).
"""
from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import build_web  # noqa: E402

passed, failed = 0, []


def check(name: str, cond: bool, detail: str = "") -> None:
    global passed
    if cond:
        passed += 1
        print(f"  ✔ {name}")
    else:
        failed.append(name + (f" — {detail}" if detail else ""))
        print(f"  ✘ {name}{' — ' + detail if detail else ''}")


def node_syntax_ok(code: str) -> tuple[bool, str]:
    """Проверить JS-блок через `node --check` (stdin)."""
    if not shutil.which("node"):
        return True, "node отсутствует — пропущено"
    with tempfile.NamedTemporaryFile("w", suffix=".js", delete=False, encoding="utf-8") as f:
        f.write(code)
        tmp = f.name
    try:
        r = subprocess.run(["node", "--check", tmp], capture_output=True, text=True, timeout=90)
        return r.returncode == 0, (r.stderr or "").strip()[:400]
    finally:
        os.unlink(tmp)


def main() -> int:
    print("\n[1] Сборка offline-бандла")
    out = os.path.join(tempfile.mkdtemp(), "CHIKENGUN_OFFLINE.html")
    build_web.build(out, with_sounds=False)
    html = open(out, encoding="utf-8").read()
    size_mb = os.path.getsize(out) / 1024 / 1024
    check("файл собран", os.path.isfile(out), out)
    check("размер разумный (5..40 МБ)", 5 < size_mb < 40, f"{size_mb:.2f} MB")

    print("\n[2] Встроенные ресурсы")
    check("движок встроен", "CHIKENGUN engine — inlined" in html)
    check("нет внешнего <script src=web/...>", 'src="web/webgl2_arena.js' not in html)
    check("карта ассетов присутствует", "window.__CG_ASSETS" in html)
    keys = re.findall(r'"(Data/[^"]+\.png)":\s*"data:image/', html)
    check("все ключи карты — png data: URL", len(keys) >= 27, f"keys={len(keys)}")
    check("нет URL-ов с http:// в карте ассетов",
          not re.search(r'"Data/[^"]+":\s*"http', html))
    check("фон загрузки в CSS встроен", 'url("data:image/png;base64,' in html)

    print("\n[3] Вложенные страницы (тут были баги)")
    iframes = re.findall(r"<iframe[^>]*>", html)
    check("iframe баньки присутствует", len(iframes) >= 1, f"iframes={len(iframes)}")
    inside = any("data-srcdoc=" in tag for tag in iframes)
    check("data-srcdoc ВНУТРИ тега iframe", inside,
          "атрибут выехал за '>' — iframe останется пустым")
    check("нет атрибута-сироты после тега", '"> data-srcdoc=' not in html)
    check("страницы для офлайн-ссылок вложены", "window.__CG_PAGES" in html)
    for page in ("minigames.html", "casino.html"):
        check(f"ссылка {page} переписана на inline", f'data-inline-page="{page}"' in html)
    # Встроенные страницы лежат в JS-строках, поэтому закрывающие теги в них
    # обязаны быть экранированы как <\/script> — иначе браузер обрывает блок.
    check("закрывающие теги внутри литералов экранированы", "<\\/script>" in html,
          "нет <\\/script> — риск обрыва инлайн-блока")
    check("нет сырого </script> внутри JSON-строк страниц",
          '"</script>' not in html)

    print("\n[4] Синтаксис всех инлайн-скриптов")
    scripts = re.findall(r"<script(?![^>]*\bsrc=)[^>]*>([\s\S]*?)</script>", html)
    check("найдено 5 инлайн-блоков", len(scripts) == 5, f"found={len(scripts)}")
    bad = []
    for i, code in enumerate(scripts):
        ok, err = node_syntax_ok(code)
        if not ok:
            bad.append(f"#{i}: {err.splitlines()[0] if err else 'ошибка'}")
    check("все инлайн-скрипты валидны для node", not bad, "; ".join(bad))

    print("\n[5] Запуск бандла без сервера (asset resolver)")
    # резолвер должен подставлять data: URL и не ломаться без карты
    resolver = re.search(r"function assetUrl\(url\) \{([\s\S]*?)\n    \}", html)
    check("в бандле есть assetUrl()", bool(resolver))
    check("резолвер читает window.__CG_ASSETS",
          bool(resolver) and "__CG_ASSETS" in resolver.group(1))

    print("\n[6] Идемпотентность")
    out2 = os.path.join(tempfile.mkdtemp(), "again.html")
    build_web.build(out2, with_sounds=False)
    a = open(out, encoding="utf-8").read()
    b = open(out2, encoding="utf-8").read()
    check("две сборки подряд дают одинаковый файл", a == b,
          f"{len(a)} vs {len(b)} байт")

    print("\n────────────────────────────────────────────")
    print(f"Пройдено: {passed}, провалено: {len(failed)}")
    if failed:
        for f in failed:
            print("  • " + f)
        return 1
    print("ВСЕ ТЕСТЫ ОФЛАЙН-СБОРКИ ПРОЙДЕНЫ ✔\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
