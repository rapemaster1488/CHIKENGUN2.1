#!/usr/bin/env bash
# ============================================================================
# CHIKENGUN 2.3 — Zero-Dependency One-Command Launcher (Linux & macOS)
# Runs out of the box with ZERO compiler, Urho3D, SDL2, or pip installs!
# Options:
#   ./play.sh              Auto: native build/MyShooter if present, else WebGL2 3D
#   ./play.sh --web        Force zero-dependency WebGL2 3D PBR + Banya edition
#   ./play.sh --native     Build & run native C++17 / Urho3D binary (./build.sh)
#   ./play.sh --port 9090  Use specific starting port (default: 8080)
#   ./play.sh --no-open    Do not auto-open browser tab
# ============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

MODE="auto"
PORT="${PORT:-8080}"
AUTO_OPEN=1

while [ "$#" -gt 0 ]; do
    case "$1" in
        --web)     MODE="web"; shift ;;
        --native)  MODE="native"; shift ;;
        --no-open) AUTO_OPEN=0; shift ;;
        --port)    PORT="${2:?missing port}"; shift 2 ;;
        -h|--help)
            sed -n '2,10p' "$0"
            exit 0
            ;;
        *) break ;;
    esac
done

if [ "$MODE" = "native" ]; then
    exec "$ROOT/run.sh" --native "$@"
fi

if [ "$MODE" = "auto" ] && [ -x "$ROOT/build/MyShooter" ]; then
    exec "$ROOT/build/MyShooter" "$@"
fi

open_browser() {
    local url="$1"
    [ "$AUTO_OPEN" -eq 0 ] && return 0
    (
        sleep 0.6
        if command -v xdg-open >/dev/null 2>&1 && { [ -n "${DISPLAY:-}" ] || [ -n "${WAYLAND_DISPLAY:-}" ]; }; then
            xdg-open "$url" >/dev/null 2>&1 || true
        elif command -v open >/dev/null 2>&1; then
            open "$url" >/dev/null 2>&1 || true
        fi
    ) &
}

if command -v python3 >/dev/null 2>&1; then
    if [ "$AUTO_OPEN" -eq 0 ]; then
        exec python3 "$ROOT/tools/serve.py" --host 0.0.0.0 --port "$PORT" --auto-port
    fi
    PORT_FILE="$(mktemp)"
    SRV_PID=""
    trap 'if [ -n "$SRV_PID" ]; then kill "$SRV_PID" 2>/dev/null || true; fi; rm -f "$PORT_FILE"' INT TERM EXIT
    python3 "$ROOT/tools/serve.py" --host 0.0.0.0 --port "$PORT" --auto-port --port-file "$PORT_FILE" &
    SRV_PID=$!
    for _ in $(seq 1 40); do
        if [ -s "$PORT_FILE" ]; then
            PORT="$(cat "$PORT_FILE")"
            break
        fi
        sleep 0.05
    done
    open_browser "http://127.0.0.1:${PORT}/index.html"
    wait "$SRV_PID"
    exit $?
fi

if command -v python >/dev/null 2>&1; then
    open_browser "http://127.0.0.1:${PORT}/index.html"
    if python -c 'import sys; raise SystemExit(0 if sys.version_info[0] >= 3 else 1)' 2>/dev/null; then
        exec python "$ROOT/tools/serve.py" --host 0.0.0.0 --port "$PORT" --auto-port
    fi
    echo "[CHIKENGUN 2.3] Open: http://127.0.0.1:${PORT}/index.html"
    exec python -m SimpleHTTPServer "$PORT"
fi

if command -v node >/dev/null 2>&1; then
    open_browser "http://127.0.0.1:${PORT}/index.html"
    exec node -e '
const http = require("http"), fs = require("fs"), path = require("path");
const root = process.cwd(), startPort = Number(process.env.PORT || 8080);
const mime = {
  ".html":"text/html; charset=utf-8",".js":"application/javascript; charset=utf-8",".css":"text/css",
  ".png":"image/png",".jpg":"image/jpeg",".svg":"image/svg+xml",".wav":"audio/wav",".ogg":"audio/ogg",
  ".xml":"application/xml",".json":"application/json",".ttf":"font/ttf"
};
const srv = http.createServer((req, res) => {
  const urlPath = decodeURIComponent((req.url || "/").split("?")[0]);
  let rel = path.normalize(urlPath === "/" ? "/index.html" : urlPath).replace(/^(\.\.[/\\])+/, "");
  let fp = path.join(root, rel);
  if (!fp.startsWith(root)) { res.writeHead(403); return res.end(); }
  fs.stat(fp, (err, st) => {
    if (err || !st.isFile()) { res.writeHead(404); return res.end("Not found"); }
    res.writeHead(200, { "Content-Type": mime[path.extname(fp).toLowerCase()] || "application/octet-stream", "Cache-Control": "no-cache" });
    fs.createReadStream(fp).pipe(res);
  });
});
(function tryListen(p) {
  srv.once("error", () => tryListen(p + 1));
  srv.listen(p, "0.0.0.0", () => console.log("[CHIKENGUN 2.3] Open: http://127.0.0.1:" + p + "/index.html"));
})(startPort);
'
fi

if command -v php >/dev/null 2>&1; then
    open_browser "http://127.0.0.1:${PORT}/index.html"
    echo "[CHIKENGUN 2.3] Open: http://127.0.0.1:${PORT}/index.html"
    exec php -S "0.0.0.0:${PORT}" -t "$ROOT"
fi

if command -v ruby >/dev/null 2>&1; then
    open_browser "http://127.0.0.1:${PORT}/index.html"
    echo "[CHIKENGUN 2.3] Open: http://127.0.0.1:${PORT}/index.html"
    exec ruby -run -e httpd "$ROOT" -p "$PORT"
fi

echo "[CHIKENGUN 2.3] No HTTP runtime found; opening index.html directly in browser..."
open_browser "file://$ROOT/index.html"
echo "Open file://$ROOT/index.html in any WebGL2 browser."
