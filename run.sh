#!/usr/bin/env bash
# Быстрый запуск CHIKENGUN 2.3 [4K ULTRA EDITION]:
# 1) С уже собранным build/MyShooter (или флагом --native при наличии Urho3D) — нативный C++17 движок;
# 2) Если Urho3D/компилятор на машине не установлены (или передан --web) — запуск zero-dependency WebGL2 3D версии.
set -e
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

if [ "${1:-}" = "--web" ]; then
    exec "$ROOT/play.sh" "$@"
fi

FORCE_NATIVE=0
if [ "${1:-}" = "--native" ]; then
    FORCE_NATIVE=1
    shift
fi

has_urho3d_headers() {
    for c in "${URHO_INC:-}" "${URHO3D_HOME:-}/include" "${URHO3D_HOME:-}/build/include" \
             "$HOME/urho3d/build/include" "$HOME/urho3d_build/include" "$HOME/urho3d-local/include" \
             "/usr/local/include/Urho3D" "/usr/include/Urho3D"; do
        [ -z "$c" ] && continue
        if [ -f "$c/Urho3D/Urho3D.h" ] || [ -f "$c/Urho3D/Engine/Application.h" ] || \
           [ -f "$c/Core/UrhoVersion.h" ] || [ -f "$c/Urho3D.h" ]; then
            return 0
        fi
    done
    return 1
}

NEED_BUILD=0
if [ ! -x "$ROOT/build/MyShooter" ]; then
    NEED_BUILD=1
else
    for f in "$ROOT/src/"*; do
        if [ "$f" -nt "$ROOT/build/MyShooter" ]; then
            NEED_BUILD=1
            break
        fi
    done
fi

if [ "$NEED_BUILD" -eq 1 ]; then
    if [ "$FORCE_NATIVE" -eq 1 ] || has_urho3d_headers; then
        echo "[run] Бинарник отсутствует или устарел — запускаю нативную сборку..."
        "$ROOT/build.sh"
    else
        echo "[run] Urho3D SDK не установлен — запускаю WebGL2 3D версию без зависимостей (./play.sh)..."
        exec "$ROOT/play.sh" --web "$@"
    fi
fi

exec "$ROOT/build/MyShooter" "$@"
