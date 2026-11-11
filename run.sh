#!/usr/bin/env bash
# ============================================================================
# Единый запуск CHIKENGUN 2.5 «ПЕРО СУДЬБЫ» (Нативный C++17 / Urho3D клиент)
# На чистой машине сам подготовит сборку (при первом запуске).
# Флаги:
#   --install-deps   Поставить системные пакеты сборки автоматически
#   --4k             Пересобрать PBR текстуры в 4096x4096
#   --rebuild        Принудительно пересобрать клиент
# ============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

FORCE_BUILD=0
BUILD_ARGS=()
APP_ARGS=()

while [ "$#" -gt 0 ]; do
    case "$1" in
        --install-deps|--4k)
            BUILD_ARGS+=("$1")
            FORCE_BUILD=1
            shift
            ;;
        --rebuild)
            FORCE_BUILD=1
            shift
            ;;
        *)
            APP_ARGS+=("$1")
            shift
            ;;
    esac
done

NEED_BUILD="$FORCE_BUILD"
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
    echo "[CHIKENGUN 2.5] Сборка / обновление нативного клиента..."
    "$ROOT/build.sh" "${BUILD_ARGS[@]}"
fi

# Пробрасываем libUrho3D.so если бинарник собран с динамическим движком
for ld in "$ROOT/.cache/urho3d-sdk/lib" "${URHO3D_HOME:-}/lib" "${URHO3D_HOME:-}/build/lib" "$HOME/urho3d/build/lib" "$HOME/urho3d_build/lib"; do
    [ -n "$ld" ] && [ -d "$ld" ] && export LD_LIBRARY_PATH="${ld}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
done

cd "$ROOT/build"
exec "$ROOT/build/MyShooter" "${APP_ARGS[@]}"
