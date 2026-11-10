#!/usr/bin/env python3
"""Zero-dependency stdlib HTTP server for CHIKENGUN 2.3 (WebGL2 3D PBR + Banya).

Binds to 0.0.0.0 so it works identically on local desktop browsers, LAN,
and cloud preview proxies without requiring any external packages.
"""

from __future__ import annotations

import argparse
import os
import socket
import sys
from functools import partial
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

EXTRA_MIME_TYPES = {
    ".html": "text/html; charset=utf-8",
    ".htm": "text/html; charset=utf-8",
    ".js": "application/javascript; charset=utf-8",
    ".mjs": "application/javascript; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".json": "application/json; charset=utf-8",
    ".xml": "application/xml; charset=utf-8",
    ".pfx": "application/xml; charset=utf-8",
    ".svg": "image/svg+xml",
    ".png": "image/png",
    ".jpg": "image/jpeg",
    ".jpeg": "image/jpeg",
    ".webp": "image/webp",
    ".wav": "audio/wav",
    ".ogg": "audio/ogg",
    ".mp3": "audio/mpeg",
    ".ttf": "font/ttf",
    ".otf": "font/otf",
    ".woff": "font/woff",
    ".woff2": "font/woff2",
    ".wasm": "application/wasm",
}


class ChikenGunHandler(SimpleHTTPRequestHandler):
    extensions_map = {**SimpleHTTPRequestHandler.extensions_map, **EXTRA_MIME_TYPES}

    def end_headers(self) -> None:
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Access-Control-Allow-Origin", "*")
        super().end_headers()

    def guess_type(self, path: str) -> str:
        _, ext = os.path.splitext(path)
        ext = ext.lower()
        if ext in EXTRA_MIME_TYPES:
            return EXTRA_MIME_TYPES[ext]
        return super().guess_type(path)

    def send_head(self):
        path = self.translate_path(self.path)
        if os.path.isdir(path):
            index_path = os.path.join(path, "index.html")
            if os.path.exists(index_path):
                path = index_path
            else:
                return super().send_head()

        if not os.path.isfile(path):
            self.send_error(HTTPStatus.NOT_FOUND, "File not found")
            return None

        range_header = self.headers.get("Range")
        if not range_header or not range_header.startswith("bytes="):
            return super().send_head()

        try:
            size = os.path.getsize(path)
            spec = range_header[6:].split(",")[0].strip()
            start_s, end_s = spec.split("-", 1)
            start = int(start_s) if start_s else 0
            end = int(end_s) if end_s else size - 1
            if start < 0 or end >= size or start > end:
                self.send_error(HTTPStatus.REQUESTED_RANGE_NOT_SATISFIABLE)
                return None

            f = open(path, "rb")
            f.seek(start)
            self.send_response(HTTPStatus.PARTIAL_CONTENT)
            self.send_header("Content-Type", self.guess_type(path))
            self.send_header("Accept-Ranges", "bytes")
            self.send_header("Content-Range", f"bytes {start}-{end}/{size}")
            self.send_header("Content-Length", str(end - start + 1))
            self.end_headers()
            return _RangeFile(f, end - start + 1)
        except Exception:
            return super().send_head()

    def log_message(self, fmt: str, *args) -> None:
        # Keep console clean; only log errors (4xx/5xx except favicon)
        if len(args) >= 2 and str(args[1]).startswith(("4", "5")):
            if "/favicon.ico" in str(args[0]):
                return
            sys.stderr.write("[serve] " + (fmt % args) + "\n")


class _RangeFile:
    def __init__(self, file_obj, remaining: int) -> None:
        self._file = file_obj
        self._remaining = remaining

    def read(self, n: int = -1) -> bytes:
        if self._remaining <= 0:
            return b""
        to_read = self._remaining if n < 0 else min(n, self._remaining)
        data = self._file.read(to_read)
        self._remaining -= len(data)
        return data

    def close(self) -> None:
        self._file.close()


class ReusableThreadingHTTPServer(ThreadingHTTPServer):
    allow_reuse_address = True
    daemon_threads = True

    def handle_error(self, request, client_address) -> None:
        _, exc, _ = sys.exc_info()
        if isinstance(exc, (BrokenPipeError, ConnectionResetError)):
            return
        super().handle_error(request, client_address)


def find_free_port(host: str, start_port: int, span: int = 30) -> int:
    for port in range(start_port, start_port + span):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            try:
                s.bind((host, port))
                return port
            except OSError:
                continue
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((host, 0))
        return int(s.getsockname()[1])


def main() -> int:
    parser = argparse.ArgumentParser(description="CHIKENGUN 2.3 local HTTP server")
    parser.add_argument("--host", default=os.environ.get("HOST", "0.0.0.0"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("PORT", "8080")))
    parser.add_argument("--auto-port", action="store_true", help="Pick next free port if busy")
    parser.add_argument("--port-file", default="", help="Write chosen port to file once bound")
    args = parser.parse_args()

    port = find_free_port(args.host, args.port) if args.auto_port else args.port
    handler = partial(ChikenGunHandler, directory=ROOT_DIR)
    server = ReusableThreadingHTTPServer((args.host, port), handler)

    if args.port_file:
        with open(args.port_file, "w", encoding="utf-8") as pf:
            pf.write(str(port))

    print(f"[CHIKENGUN 2.3] Serving {ROOT_DIR}", flush=True)
    print(f"[CHIKENGUN 2.3] Open: http://127.0.0.1:{port}/index.html", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[CHIKENGUN 2.3] Server stopped.", flush=True)
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
