# syntax=docker/dockerfile:1
# ============================================================================
# CHIKENGUN 2.5 — ПЕРО СУДЬБЫ (официальный Docker-образ)
#
# Многостадийная сборка нативного C++17 / Urho3D клиента:
#   1) builder  — ставит тулчейн, собирает кэш Urho3D 1.9.0, PBR-текстуры,
#                 звуки и бинарник build/MyShooter (полный прогон build.sh);
#   2) runtime  — минимальный Debian с Mesa (llvmpipe), Xvfb + x11vnc + noVNC:
#                 игра доступна в БРАУЗЕРЕ на http://localhost:6080/ с любой ОС.
#
# Сборка:        docker compose build  (или ./docker.sh build)
# 4K-текстуры:   TEX_SIZE=4096 docker compose build  (или ./docker.sh build --4k)
#
# Для сборки образа нужен доступ в сеть (git clone urho3d/Urho3D) и BuildKit
# (Docker 20.10+ / Compose v2 — включены по умолчанию).
# ============================================================================

# ---------------------------------------------------------------------------
# Стадия 1: сборка нативного клиента
# ---------------------------------------------------------------------------
FROM debian:12-slim AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Системные зависимости сборки — тот же набор, что в build.sh / CI (apt).
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake git ca-certificates pkg-config python3 \
        libsdl2-dev libgl-dev libegl-dev libglu1-mesa-dev \
        libx11-dev libxrandr-dev libxi-dev libxext-dev libxcursor-dev \
        libxinerama-dev libxss-dev libxcb1-dev \
        libwayland-dev wayland-protocols libxkbcommon-dev \
        libasound2-dev libpulse-dev libdbus-1-dev libudev-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# .dockerignore исключает .git / build / .cache — копируются только исходники.
COPY . .

# TEX_SIZE=1024 (быстро), TEX_SIZE=4096 (полный 4K UHD набор PBR-текстур).
ARG TEX_SIZE=1024
ENV TEX_SIZE=${TEX_SIZE} \
    CHIKENGUN_ALLOW_SILENT_BUILD=1

# Кэш-маунт BuildKit держит собранный один раз Urho3D SDK между сборками
# образа (.cache/urho3d-sdk), поэтому правки src/* не пересобирают движок.
# Валидация линковки (ldd) и SDL-диагностика отсекают «сломанный» образ сразу.
RUN --mount=type=cache,target=/src/.cache \
    set -eux; \
    ./build.sh -j"$(nproc)"; \
    ldd build/MyShooter > /tmp/ldd.out; \
    cat /tmp/ldd.out; \
    if grep -q "not found" /tmp/ldd.out; then \
        echo "ОШИБКА: в бинарнике есть неразрешённые библиотеки"; exit 1; \
    fi; \
    { env -u DISPLAY -u WAYLAND_DISPLAY ./build/MyShooter --diagnose-sdl > /tmp/diag.out 2>&1 || true; }; \
    grep -q "Video drivers (built):" /tmp/diag.out; \
    test -f build/Data/Textures/Skybox4K.xml; \
    test -d build/CoreData/Shaders

# ---------------------------------------------------------------------------
# Стадия 2: runtime — Mesa + Xvfb + x11vnc + noVNC (игра в браузере)
# ---------------------------------------------------------------------------
FROM debian:12-slim AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# Mesa (программный рендеринг llvmpipe), X11/GLX-клиентские библиотеки,
# ALSA/PulseAudio (могут дозагружаться SDL), Xvfb + VNC + noVNC для веб-режима.
RUN apt-get update && apt-get install -y --no-install-recommends \
        libgl1 libgl1-mesa-dri libglx-mesa0 libegl1 libglu1-mesa \
        libx11-6 libxext6 libxrandr2 libxi6 libxcursor1 libxinerama1 \
        libxss1 libxcb1 libxxf86vm1 libxrender1 libxfixes3 \
        libxkbcommon0 libwayland-client0 libwayland-cursor0 libwayland-egl1 \
        libasound2 libpulse0 libdbus-1-3 libudev1 \
        xvfb x11vnc novnc websockify \
        ca-certificates procps mesa-utils \
    && rm -rf /var/lib/apt/lists/*

# Непривилегированный пользователь + точка монтирования сохранений (/data).
RUN useradd --create-home --uid 1000 chikengun \
    && mkdir -p /data /opt/chikengun \
    && chown -R chikengun:chikengun /data /opt/chikengun

WORKDIR /opt/chikengun

COPY --from=builder --chown=chikengun:chikengun /src/build/MyShooter  /opt/chikengun/MyShooter
COPY --from=builder --chown=chikengun:chikengun /src/build/Data       /opt/chikengun/Data
COPY --from=builder --chown=chikengun:chikengun /src/build/CoreData   /opt/chikengun/CoreData
COPY --chown=chikengun:chikengun bin/settings.ini                      /opt/chikengun/settings.ini
COPY docker/entrypoint.sh /usr/local/bin/chikengun-entrypoint

RUN chmod +x /usr/local/bin/chikengun-entrypoint /opt/chikengun/MyShooter

USER chikengun
ENV HOME=/home/chikengun \
    CHIKENGUN_GAME_DIR=/opt/chikengun \
    CHIKENGUN_DATA_DIR=/opt/chikengun/Data \
    CHIKENGUN_COREDATA_DIR=/opt/chikengun/CoreData \
    CHIKENGUN_MODE=web \
    LIBGL_ALWAYS_SOFTWARE=1

# 6080 — noVNC (браузер), 5900 — прямой VNC-клиент (по желанию).
EXPOSE 6080 5900

ENTRYPOINT ["/usr/local/bin/chikengun-entrypoint"]
