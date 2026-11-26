#!/usr/bin/env bash
# ============================================================================
# check_web.sh — быстрая проверка веб-части CHIKENGUN (движок + офлайн-сборка).
# Без внешних зависимостей: нужен только node (для тестов движка) и python3.
#
#   ./tools/check_web.sh          # всё
#   ./tools/check_web.sh --fast   # только синтаксис + тесты движка, без сборки бандла
# ============================================================================
set -eo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
FAST=0
[ "${1:-}" = "--fast" ] && FAST=1

echo "[web] 1/4 Синтаксис движка (node --check)..."
if command -v node >/dev/null 2>&1; then
    node --check web/webgl2_arena.js
    echo "[web] OK"
else
    echo "[web] node не найден — пропускаю"
fi

echo "[web] 2/4 Синтаксис сборщика (python)..."
python3 -c "import ast,sys; ast.parse(open('tools/build_web.py').read())"

if [ "$FAST" = "0" ]; then
    echo "[web] 3/4 Тесты офлайн-сборки..."
    python3 tests/web/test_offline_build.py | tail -3
else
    echo "[web] 3/4 Тесты офлайн-сборки — пропущено (--fast)"
fi

echo "[web] 4/4 Тесты движка (stub WebGL2)..."
if command -v node >/dev/null 2>&1; then
    node tests/web/run_tests.js | tail -3
else
    echo "[web] node не найден — пропускаю"
fi

echo "[web] ВСЁ ЗЕЛЁНОЕ ✔"
