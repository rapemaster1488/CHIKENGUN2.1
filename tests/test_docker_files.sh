#!/usr/bin/env bash
# Static contract checks for the Docker packaging (Dockerfile, compose, entrypoint).
# Runs without a Docker daemon: suitable for CI, ctest and hosts without Docker.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

fail() {
    printf 'Docker packaging check FAILED: %s\n' "$1" >&2
    exit 1
}

require_file() {
    [ -f "$root/$1" ] || fail "missing file: $1"
}

require_grep() {
    # require_grep <pattern> <file> <hint>
    grep -Eq -- "$1" "$root/$2" || fail "$2: $3"
}

# ---- 1. Обязательные файлы ----
for f in Dockerfile docker-compose.yml .dockerignore docker/entrypoint.sh docker.sh docs/DOCKER.md; do
    require_file "$f"
done

# ---- 2. Синтаксис всех Docker-скриптов ----
bash -n "$root/docker.sh" || fail "docker.sh has a bash syntax error"
bash -n "$root/docker/entrypoint.sh" || fail "docker/entrypoint.sh has a bash syntax error"

# ---- 3. Контракт Dockerfile ----
require_grep 'AS builder' 'Dockerfile' 'builder stage is required'
require_grep 'AS runtime' 'Dockerfile' 'runtime stage is required'
require_grep 'ARG TEX_SIZE' 'Dockerfile' 'TEX_SIZE build-arg is required (1024 default, 4096 for 4K)'
require_grep 'build/MyShooter' 'Dockerfile' 'the native client must be copied from the builder stage'
require_grep 'EXPOSE 6080' 'Dockerfile' 'noVNC port 6080 must be exposed'
require_grep 'chikengun-entrypoint' 'Dockerfile' 'the shared entrypoint must be installed'
require_grep 'useradd .*chikengun' 'Dockerfile' 'the runtime must use a non-root user'
require_grep 'libgl1-mesa-dri' 'Dockerfile' 'software GL (llvmpipe) is required for the web mode'
require_grep 'xvfb' 'Dockerfile' 'Xvfb is required for the web mode'
require_grep 'novnc' 'Dockerfile' 'noVNC is required for the web mode'

# ---- 4. Контракт .dockerignore (кэш SDK и сборка не должны попадать в контекст) ----
require_grep '^\.git$' '.dockerignore' '.git must not enter the build context'
require_grep '^\.cache' '.dockerignore' '.cache (Urho3D SDK cache) must not enter the build context'
require_grep '^build' '.dockerignore' 'build outputs must not enter the build context'

# ---- 5. Контракт docker-compose.yml ----
require_grep '^  chikengun:' 'docker-compose.yml' 'web-mode service "chikengun" is required'
require_grep '^  chikengun-x11:' 'docker-compose.yml' 'x11-mode service "chikengun-x11" is required'
require_grep 'profiles:.*"x11"' 'docker-compose.yml' 'the x11 service must sit behind the "x11" profile'
require_grep '"\$\{NOVNC_PORT:-6080\}:6080"' 'docker-compose.yml' 'noVNC port mapping is required'
require_grep 'CHIKENGUN_MODE: "web"' 'docker-compose.yml' 'the default service must run the web mode'
require_grep '/tmp/.X11-unix:/tmp/.X11-unix' 'docker-compose.yml' 'the x11 service must mount the host X11 socket'
require_grep 'chikengun-save:/data' 'docker-compose.yml' 'settings.ini persistence volume is required'
require_grep 'shm_size' 'docker-compose.yml' 'shm_size must be raised for Xvfb/llvmpipe'

# ---- 6. Контракт entrypoint ----
require_grep 'CHIKENGUN_MODE' 'docker/entrypoint.sh' 'entrypoint must honour CHIKENGUN_MODE'
require_grep 'web\)' 'docker/entrypoint.sh' 'web mode is required'
require_grep 'x11\)' 'docker/entrypoint.sh' 'x11 mode is required'
require_grep 'offscreen\)' 'docker/entrypoint.sh' 'offscreen mode is required'
require_grep 'VNC_PASSWORD' 'docker/entrypoint.sh' 'optional VNC authentication is required'
require_grep 'settings\.ini' 'docker/entrypoint.sh' 'settings.ini save/restore is required'
require_grep 'x11vnc' 'docker/entrypoint.sh' 'a VNC server is required for the web mode'
require_grep 'websockify' 'docker/entrypoint.sh' 'websockify/noVNC bridge is required for the web mode'

# ---- 7. README должен вести в Docker-документацию ----
require_grep 'docs/DOCKER\.md' 'README.md' 'README must link the Docker guide'
require_grep 'docker\.sh' 'README.md' 'README must mention ./docker.sh'

echo "Docker packaging checks passed (Dockerfile, compose, entrypoint, docs)"
