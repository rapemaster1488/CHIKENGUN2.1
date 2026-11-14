#!/usr/bin/env bash
# ============================================================================
# CHIKENGUN 2.5 — единая Docker-обёртка (работает везде: Linux, Windows, macOS)
#
# Флаги / команды:
#   (без аргументов)   собрать образ и запустить игру в БРАУЗЕРЕ на http://localhost:6080/
#   run [аргументы]    одноразовый запуск с аргументами игры (например: run -nosound)
#   x11                нативное окно через X11 хоста (Linux; требует xhost +local:)
#   build              только собрать образ
#   --4k               полный 4K UHD набор PBR-текстур (TEX_SIZE=4096)
#   shell              отладочный шелл внутри образа
#   down               остановить и удалить контейнеры/тома compose
#   help               эта справка
#
# Примеры:
#   ./docker.sh                  # игра в браузере
#   ./docker.sh build --4k       # образ с 4K-текстурами
#   VNC_PASSWORD=секрет ./docker.sh   # с VNC-паролем (публичные серверы)
#   ./docker.sh x11              # нативное окно на Linux
# ============================================================================
if [ -z "${BASH_VERSION:-}" ]; then exec bash "$0" "$@"; fi
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

# ---- Выбор Docker-совместимого инструмента (docker compose / docker-compose / podman) ----
detect_compose() {
    if command -v docker >/dev/null 2>&1 && docker compose version >/dev/null 2>&1; then
        echo "docker compose"
    elif command -v docker-compose >/dev/null 2>&1; then
        echo "docker-compose"
    elif command -v podman >/dev/null 2>&1 && podman compose version >/dev/null 2>&1; then
        echo "podman compose"
    else
        echo ""
    fi
}

# ---- Разбор аргументов ----
CMD="up"
EXTRA_ARGS=()
while [ "$#" -gt 0 ]; do
    case "$1" in
        up|run|x11|build|shell|down|help) CMD="$1"; shift ;;
        --4k) export TEX_SIZE=4096; shift ;;
        --rebuild) EXTRA_ARGS+=("--build"); shift ;;
        -h|--help) CMD="help"; shift ;;
        *) EXTRA_ARGS+=("$1"); shift ;;
    esac
done

if [ "$CMD" = "help" ]; then
    sed -n '2,20p' "$0" | sed 's/^# \?//'
    exit 0
fi

COMPOSE="$(detect_compose)"
if [ -z "$COMPOSE" ]; then
    cat >&2 <<'MSG'
ОШИБКА: не найден Docker (или Podman) с Compose.

Установите Docker Engine + Compose v2:
  https://docs.docker.com/engine/install/
или Podman:
  https://podman.io/docs/installation

Проверка:  docker compose version
MSG
    exit 1
fi

case "$CMD" in
    up|run)
        if [ "$CMD" = "run" ]; then
            # Одноразовый запуск с аргументами игры: ./docker.sh run -nosound
            echo "[docker.sh] Одноразовый запуск CHIKENGUN 2.5 (веб-режим noVNC)..."
            exec $COMPOSE run --rm --build chikengun "${EXTRA_ARGS[@]+${EXTRA_ARGS[@]}}"
        fi
        echo "[docker.sh] Собираю образ и запускаю CHIKENGUN 2.5 (веб-режим noVNC)..."
        echo "[docker.sh] Игра откроется в браузере: http://localhost:6080/"
        exec $COMPOSE up --build "${EXTRA_ARGS[@]+${EXTRA_ARGS[@]}}"
        ;;
    build)
        echo "[docker.sh] Собираю Docker-образ (TEX_SIZE=${TEX_SIZE:-1024})..."
        exec $COMPOSE build "${EXTRA_ARGS[@]+${EXTRA_ARGS[@]}}"
        ;;
    x11)
        if [ -z "${DISPLAY:-}" ]; then
            echo "ОШИБКА: для режима x11 нужен работающий X11/XWayland (DISPLAY не задан)." >&2
            exit 1
        fi
        # Разрешить локальному Docker-контейнеру доступ к X-серверу хоста.
        xhost +local:docker >/dev/null 2>&1 || xhost +local: >/dev/null 2>&1 || true
        echo "[docker.sh] Нативный X11-режим: дисплей ${DISPLAY}"
        exec $COMPOSE --profile x11 run --rm chikengun-x11 "${EXTRA_ARGS[@]+${EXTRA_ARGS[@]}}"
        ;;
    shell)
        exec $COMPOSE run --rm --entrypoint bash chikengun "${EXTRA_ARGS[@]+${EXTRA_ARGS[@]}}"
        ;;
    down)
        exec $COMPOSE --profile x11 --profile tools down "${EXTRA_ARGS[@]+${EXTRA_ARGS[@]}}"
        ;;
esac
