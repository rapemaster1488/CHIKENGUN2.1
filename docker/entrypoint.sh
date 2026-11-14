#!/usr/bin/env bash
# ============================================================================
# CHIKENGUN 2.5 — единая Docker-точка входа.
#
# Режимы (CHIKENGUN_MODE):
#   web       (по умолчанию) — Xvfb + x11vnc + noVNC: игра в БРАУЗЕРЕ
#             на http://localhost:6080/ с любой ОС (Linux/Windows/macOS).
#   x11       — игра в нативном окне через X11-сокет хоста (Linux; звук — см. docs).
#   offscreen — только Xvfb, без VNC (проверки / скриншоты в контейнере).
#
# Переменные:
#   CHIKENGUN_RESOLUTION   геометрия дисплея Xvfb (default 1280x720x24)
#   CHIKENGUN_NOVNC_PORT   порт noVNC (default 6080)
#   CHIKENGUN_VNC_PORT     порт VNC (default 5900)
#   VNC_PASSWORD           если задан — включает VNC-аутентификацию
#   LIBGL_ALWAYS_SOFTWARE  1 = llvmpipe (default), 0 = GPU через /dev/dri
#   CHIKENGUN_SAVE_DIR     каталог сохранений settings.ini (default /data)
#
# Все переданные аргументы уходят в MyShooter (например -nosound).
# Для отладки: docker compose run --rm chikengun bash
# ============================================================================
set -euo pipefail

GAME_DIR="${CHIKENGUN_GAME_DIR:-/opt/chikengun}"
SAVE_DIR="${CHIKENGUN_SAVE_DIR:-/data}"
MODE="${CHIKENGUN_MODE:-web}"
DISPLAY_NUM="${CHIKENGUN_DISPLAY_NUM:-:99}"
RESOLUTION="${CHIKENGUN_RESOLUTION:-1280x720x24}"
NOVNC_PORT="${CHIKENGUN_NOVNC_PORT:-6080}"
VNC_PORT="${CHIKENGUN_VNC_PORT:-5900}"

# Отладочный шелл вместо игры.
if [ "${1:-}" = "bash" ] || [ "${1:-}" = "sh" ]; then
    exec "$@"
fi

echo "================================================================"
echo " CHIKENGUN 2.5 «ПЕРО СУДЬБЫ» — Docker (${MODE})"
echo "================================================================"

# ---- Сохранения (settings.ini: кредиты ¤, графика, достижения) ----
mkdir -p "$SAVE_DIR" 2>/dev/null || true
if [ -f "$SAVE_DIR/settings.ini" ]; then
    cp -f "$SAVE_DIR/settings.ini" "$GAME_DIR/settings.ini" 2>/dev/null || true
fi

save_settings() {
    if [ -d "$SAVE_DIR" ] && [ -w "$SAVE_DIR" ] && [ -f "$GAME_DIR/settings.ini" ]; then
        cp -f "$GAME_DIR/settings.ini" "$SAVE_DIR/settings.ini" 2>/dev/null || true
    fi
}
trap save_settings EXIT

# ---- Виртуальный дисплей Xvfb ----
start_xvfb() {
    echo "[docker] Xvfb: дисплей ${DISPLAY_NUM}, разрешение ${RESOLUTION}..."
    Xvfb "$DISPLAY_NUM" -screen 0 "$RESOLUTION" -ac +extension GLX +render -noreset -nolisten tcp \
        >/tmp/xvfb.log 2>&1 &
    XVFB_PID=$!
    i=0
    while [ "$i" -lt 50 ]; do
        if [ -e "/tmp/.X11-unix/X${DISPLAY_NUM#:}" ]; then
            break
        fi
        if ! kill -0 "$XVFB_PID" 2>/dev/null; then
            echo "ОШИБКА: Xvfb не запустился:" >&2
            cat /tmp/xvfb.log >&2 || true
            exit 1
        fi
        i=$((i + 1))
        sleep 0.2
    done
    export DISPLAY="$DISPLAY_NUM"
}

case "$MODE" in
    web)
        start_xvfb
        export LIBGL_ALWAYS_SOFTWARE="${LIBGL_ALWAYS_SOFTWARE:-1}"

        # VNC-сервер (для прямых VNC-клиентов и как транспорт noVNC).
        VNC_ARGS=(-display "$DISPLAY_NUM" -forever -shared -rfbport "$VNC_PORT")
        if [ -n "${VNC_PASSWORD:-}" ]; then
            x11vnc -storepasswd "$VNC_PASSWORD" /tmp/vncpwd >/dev/null 2>&1
            VNC_ARGS+=(-rfbauth /tmp/vncpwd)
            echo "[docker] VNC: порт ${VNC_PORT} (аутентификация включена)"
        else
            VNC_ARGS+=(-nopw)
            echo "[docker] VNC: порт ${VNC_PORT} (без пароля; задайте VNC_PASSWORD для публичных серверов)"
        fi
        x11vnc "${VNC_ARGS[@]}" >/tmp/x11vnc.log 2>&1 &

        # noVNC: статика копируется в HOME, чтобы добавить index.html с автоподключением.
        NOVNC_ROOT="${HOME}/novnc"
        if [ ! -d "$NOVNC_ROOT" ] && [ -d /usr/share/novnc ]; then
            cp -r /usr/share/novnc "$NOVNC_ROOT"
        fi
        if [ -d "$NOVNC_ROOT" ] && [ ! -f "$NOVNC_ROOT/index.html" ]; then
            cat > "$NOVNC_ROOT/index.html" <<'HTML'
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="utf-8">
<title>CHIKENGUN 2.5 — ПЕРО СУДЬБЫ</title>
<meta http-equiv="refresh" content="0; url=vnc.html?autoconnect=true&resize=scale&path=websockify">
</head>
<body style="background:#0b0e14;color:#e8e8e8;font-family:sans-serif;display:flex;align-items:center;justify-content:center;height:100vh;margin:0">
<div style="text-align:center">
<h1>CHIKENGUN 2.5 — ПЕРО СУДЬБЫ</h1>
<p>Подключение к игровому дисплею&hellip;</p>
<p><a style="color:#7fd0ff" href="vnc.html?autoconnect=true&resize=scale&path=websockify">Открыть вручную</a></p>
</div>
</body>
</html>
HTML
        fi

        if [ -d "$NOVNC_ROOT" ]; then
            echo "[docker] noVNC: http://localhost:${NOVNC_PORT}/ (кликните по игровому окну для захвата клавиатуры)"
            websockify --web="$NOVNC_ROOT" "0.0.0.0:${NOVNC_PORT}" "localhost:${VNC_PORT}" \
                >/tmp/websockify.log 2>&1 &
        else
            echo "[docker] noVNC не найден — только прямой VNC на порту ${VNC_PORT}"
            websockify "0.0.0.0:${NOVNC_PORT}" "localhost:${VNC_PORT}" \
                >/tmp/websockify.log 2>&1 &
        fi
        ;;
    x11)
        if [ -z "${DISPLAY:-}" ]; then
            echo "ОШИБКА: CHIKENGUN_MODE=x11 требует DISPLAY и проброшенного /tmp/.X11-unix." >&2
            echo "       На хосте: xhost +local: && docker compose --profile x11 run --rm chikengun-x11" >&2
            exit 1
        fi
        echo "[docker] Нативный режим X11: дисплей хоста ${DISPLAY}"
        ;;
    offscreen)
        start_xvfb
        export LIBGL_ALWAYS_SOFTWARE="${LIBGL_ALWAYS_SOFTWARE:-1}"
        echo "[docker] Offscreen-режим: Xvfb без VNC (проверки / скриншоты)."
        ;;
    *)
        echo "ОШИБКА: неизвестный CHIKENGUN_MODE='${MODE}' (допустимо: web, x11, offscreen)." >&2
        exit 2
        ;;
esac

# ---- Запуск игры (все аргументы — в MyShooter) ----
echo "[docker] Запуск MyShooter $*"
set +e
"$GAME_DIR/MyShooter" "$@" &
GAME_PID=$!
trap 'save_settings; kill -TERM "$GAME_PID" 2>/dev/null' TERM INT
wait "$GAME_PID"
GAME_EXIT=$?
set -e
save_settings
trap - EXIT TERM INT
echo "[docker] Игра завершилась (код ${GAME_EXIT})."
exit "$GAME_EXIT"
