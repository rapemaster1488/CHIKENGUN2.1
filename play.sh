#!/usr/bin/env bash
# ============================================================================
# CHIKENGUN 2.4 — ЛОКАЛЬНЫЙ ЗАПУСК НА ПК
# Без интернета, без установки софта (нужен только Python 3, он есть в Linux/macOS,
# на Windows — python.org или Microsoft Store).
#
#   ./play.sh              # пересобрать офлайн-сборку (если нужно) и открыть в браузере
#   ./play.sh --server     # локальный сервер http://127.0.0.1:8080 (как превью)
#   ./play.sh --rebuild    # принудительно пересобрать CHIKENGUN_OFFLINE.html
#   ./play.sh --native     # собрать и запустить нативный клиент Urho3D (./run.sh)
# ============================================================================
set -e
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

MODE="offline"
REBUILD=0
for arg in "$@"; do
  case "$arg" in
    --server)  MODE="server" ;;
    --native)  MODE="native" ;;
    --rebuild) REBUILD=1 ;;
    -h|--help) sed -n '2,12p' "$0"; exit 0 ;;
    *) echo "[play] неизвестный аргумент: $arg"; exit 1 ;;
  esac
done

open_url() {
  local url="$1"
  if command -v xdg-open >/dev/null 2>&1; then xdg-open "$url" >/dev/null 2>&1 || true
  elif command -v open     >/dev/null 2>&1; then open "$url"     >/dev/null 2>&1 || true
  elif command -v start    >/dev/null 2>&1; then start "" "$url" >/dev/null 2>&1 || true
  else echo "[play] открой вручную: $url"; fi
}

if [ "$MODE" = "native" ]; then
  echo "[play] Нативный клиент Urho3D: ./run.sh"
  if ! ./run.sh; then
    cat <<'EOT'

[play] ────────────────────────────────────────────────────────────────
[play] Нативной сборке не хватает движка Urho3D (это внешняя зависимость,
[play] её нельзя вшить в репозиторий). Варианты:

[play] 1) Просто поиграть ПРЯМО СЕЙЧАС, без установки чего-либо:
[play]        ./play.sh                ← офлайн-сборка, открывается в браузере
[play]        ./play.sh --server       ← то же, но через локальный сервер

[play] 2) Собрать нативный клиент (как раньше):
[play]        git clone --depth 1 https://github.com/urho3d/Urho3D.git ~/urho3d
[play]        cmake -B ~/urho3d/build -S ~/urho3d -DCMAKE_BUILD_TYPE=Release -DURHO3D_SAMPLES=OFF
[play]        cmake --build ~/urho3d/build -j
[play]        URHO3D_HOME=~/urho3d ./run.sh
[play] ────────────────────────────────────────────────────────────────
EOT
    exit 1
  fi
  exit 0
fi

if [ "$MODE" = "server" ]; then
  PORT="${PORT:-8080}"
  # если порт занят — молча берём следующий свободный
  for _ in 1 2 3 4 5 6 7 8 9 10; do
    if ! ( exec 3<>"/dev/tcp/127.0.0.1/$PORT" ) 2>/dev/null; then break; fi
    exec 3>&- 2>/dev/null || true
    PORT=$((PORT + 1))
  done
  echo "[play] Сервер: http://127.0.0.1:$PORT/  (Ctrl+C — остановить)"
  ( sleep 1; open_url "http://127.0.0.1:$PORT/" ) &
  exec python3 -m http.server "$PORT" --bind 0.0.0.0 --directory "$ROOT"
fi

OUT="$ROOT/CHIKENGUN_OFFLINE.html"
if [ "$REBUILD" = "1" ] || [ ! -f "$OUT" ] || [ "$ROOT/index.html" -nt "$OUT" ] || [ "$ROOT/web/webgl2_arena.js" -nt "$OUT" ]; then
  echo "[play] Сборка офлайн-версии (один файл, без сервера)..."
  python3 "$ROOT/tools/build_web.py"
fi

echo "[play] Открываю $OUT"
open_url "file://$OUT"
echo "[play] Готово. Если браузер не открылся — просто дважды кликни по CHIKENGUN_OFFLINE.html"
