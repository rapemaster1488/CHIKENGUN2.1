#!/usr/bin/env bash
# Быстрый запуск CHIKENGUN 2.4 [4K ULTRA C++ EDITION]:
# если бинарник ещё не собран или исходник новее — автоматически запускает ./build.sh.
set -e
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

if [ ! -x "$ROOT/build/MyShooter" ] || [ "$ROOT/src/MyApp.cpp" -nt "$ROOT/build/MyShooter" ]; then
    echo "[run] Бинарник отсутствует или устарел — запускаю сборку..."
    "$ROOT/build.sh"
fi

exec "$ROOT/build/MyShooter" "$@"
