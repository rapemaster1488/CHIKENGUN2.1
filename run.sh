#!/usr/bin/env bash
# ============================================================================
# Единый кросс-платформенный запуск CHIKENGUN 2.5 «ПЕРО СУДЬБЫ» (Native C++17)
# Работает на NixOS / Nix (локальный профиль, nix-shell и flakes), Linux, macOS и Windows MSYS2.
#
# Флаги:
#   --install-deps   Поставить системные пакеты / войти в Nix shell
#   --4k             Пересобрать PBR текстуры в 4096x4096
#   --rebuild        Принудительно пересобрать клиент
# ============================================================================
if [ -z "${BASH_VERSION:-}" ]; then exec bash "$0" "$@"; fi
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
ORIG_ARGS=("$@")
cd "$ROOT"

OS_NAME="$(uname -s)"
EXE_EXT=""
case "$OS_NAME" in
    MINGW*|MSYS*|CYGWIN*) EXE_EXT=".exe" ;;
esac

IS_NIX=0
if [ -f /etc/NIXOS ] || [ -n "${IN_NIX_SHELL:-}" ] || [ -n "${NIX_CC:-}" ] || [ -n "${NIX_CFLAGS_COMPILE:-}" ] || [ -d "$HOME/.nix-profile" ]; then
    IS_NIX=1
fi

FORCE_BUILD=0
INSTALL_DEPS=0
BUILD_ARGS=()
APP_ARGS=()

while [ "$#" -gt 0 ]; do
    case "$1" in
        --install-deps)
            INSTALL_DEPS=1
            BUILD_ARGS+=("$1")
            FORCE_BUILD=1
            shift
            ;;
        --4k)
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

if [ -z "${URHO3D_HOME:-}" ] && [ -f "$ROOT/build/.urho_sdk_home" ]; then
    saved_urho_home="$(cat "$ROOT/build/.urho_sdk_home" 2>/dev/null || true)"
    [ -n "$saved_urho_home" ] && [ -d "$saved_urho_home" ] && export URHO3D_HOME="$saved_urho_home"
fi

TARGET_BIN="$ROOT/build/MyShooter${EXE_EXT}"
if [ ! -x "$TARGET_BIN" ] && [ -x "$ROOT/build/Release/MyShooter${EXE_EXT}" ]; then
    TARGET_BIN="$ROOT/build/Release/MyShooter${EXE_EXT}"
fi

bin_missing_video_driver() {
    local bin="$1"
    [ "$OS_NAME" = "Linux" ] && [ -x "$bin" ] && [ "${URHO3D_ALLOW_HEADLESS:-0}" != "1" ] || return 1
    if command -v nm >/dev/null 2>&1; then
        # Если в бинарник вшит статический SDL_Init, но нет ни одного видеодрайвера (X11/Wayland/KMSDRM) — сборка битая.
        if nm -g --defined-only "$bin" 2>/dev/null | grep -qE ' T _?SDL_Init$'; then
            if ! nm -g --defined-only "$bin" 2>/dev/null | grep -qE ' [A-Za-z] _?(X11_bootstrap|Wayland_bootstrap|KMSDRM_bootstrap)$'; then
                return 0
            fi
        fi
    fi
    return 1
}

NEED_BUILD="$FORCE_BUILD"
if [ ! -x "$TARGET_BIN" ] || [ ! -f "$ROOT/build/Data/Textures/Skybox4K.xml" ] || [ ! -f "$ROOT/build/Data/Textures/BanyaBirchDiffuse.png" ] || [ ! -d "$ROOT/build/CoreData/Shaders" ]; then
    NEED_BUILD=1
elif bin_missing_video_driver "$TARGET_BIN"; then
    echo "[CHIKENGUN 2.5] Обнаружен бинарник, собранный с headless-сборкой Urho3D (без X11/Wayland видеодрайверов)."
    echo "                 Запускаю пересборку с полноценным Urho3D SDK..."
    NEED_BUILD=1
else
    for f in "$ROOT/src/"*; do
        if [ "$f" -nt "$TARGET_BIN" ]; then
            NEED_BUILD=1
            break
        fi
    done
fi

# На Nix/NixOS вне shell сначала проверяем: если клиент не собран и базовые инструменты отсутствуют,
# входим в быстрый nix-shell (предпочтительно, т.к. не пересобирает Urho3D с нуля) или nix develop.
if [ "$IS_NIX" -eq 1 ] && [ -z "${IN_NIX_SHELL:-}" ]; then
    NEED_NIX_SHELL="$INSTALL_DEPS"
    if [ "$NEED_BUILD" -eq 1 ] && [ "$NEED_NIX_SHELL" -eq 0 ]; then
        if ! command -v g++ >/dev/null 2>&1 && ! command -v clang++ >/dev/null 2>&1 && ! command -v c++ >/dev/null 2>&1; then
            NEED_NIX_SHELL=1
        fi
    fi
    if [ "$NEED_NIX_SHELL" -eq 1 ]; then
        if command -v nix-shell >/dev/null 2>&1 && [ -f "$ROOT/shell.nix" ] && command -v nix-instantiate >/dev/null 2>&1 && nix-instantiate --eval -E '<nixpkgs>' >/dev/null 2>&1; then
            echo "[NixOS] Запускаю внутри nix-shell..."
            exec nix-shell "$ROOT/shell.nix" --run "$(printf '%q ' "$0" ${ORIG_ARGS[@]+"${ORIG_ARGS[@]}"})"
        elif command -v nix >/dev/null 2>&1 && nix --extra-experimental-features "nix-command flakes" flake metadata "$ROOT" >/dev/null 2>&1; then
            echo "[NixOS] Запускаю внутри nix develop..."
            exec nix --extra-experimental-features "nix-command flakes" develop "$ROOT" -c "$0" ${ORIG_ARGS[@]+"${ORIG_ARGS[@]}"}
        fi
    fi
fi

if [ "$NEED_BUILD" -eq 1 ]; then
    echo "[CHIKENGUN 2.5] Сборка / обновление нативного клиента..."
    "$ROOT/build.sh" ${BUILD_ARGS[@]+"${BUILD_ARGS[@]}"}
    TARGET_BIN="$ROOT/build/MyShooter${EXE_EXT}"
fi

if [ "$OS_NAME" = "Linux" ]; then
    # Драйверы OpenGL/Vulkan на NixOS живут в /run/opengl-driver/lib + локальные профили Nix
    for ld in \
        "/run/opengl-driver/lib" \
        "/run/opengl-driver-32/lib" \
        "$ROOT/.cache/urho3d-sdk/lib" \
        "$ROOT/.cache/urho3d-sdk/lib64" \
        "${URHO3D_HOME:-}/lib" \
        "${URHO3D_HOME:-}/lib64" \
        "${URHO3D_HOME:-}/build/lib" \
        "$HOME/urho3d-build/out/lib" \
        "$HOME/urho3d-build/lib" \
        "$HOME/urho3d/build/lib" \
        "$HOME/urho3d_build/out/lib" \
        "$HOME/urho3d_build/lib" \
        "$HOME/urho3d-local/lib" \
        "$HOME/urho3d-local/lib64" \
        "$HOME/.nix-profile/lib" \
        "/run/current-system/sw/lib"
    do
        [ -n "$ld" ] && [ -d "$ld" ] && export LD_LIBRARY_PATH="${ld}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    done
elif [ "$OS_NAME" = "Darwin" ]; then
    for ld in \
        "$ROOT/.cache/urho3d-sdk/lib" \
        "${URHO3D_HOME:-}/lib" \
        "${URHO3D_HOME:-}/build/lib" \
        "$HOME/urho3d/build/lib" \
        "/opt/homebrew/lib" \
        "/usr/local/lib"
    do
        [ -n "$ld" ] && [ -d "$ld" ] && export DYLD_LIBRARY_PATH="${ld}${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
    done
fi

export CHIKENGUN_DATA_DIR="${CHIKENGUN_DATA_DIR:-$ROOT/build/Data}"
export CHIKENGUN_COREDATA_DIR="${CHIKENGUN_COREDATA_DIR:-$ROOT/build/CoreData}"

if [ "$OS_NAME" = "Linux" ]; then
    if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ] && [ "${SDL_VIDEODRIVER:-}" != "dummy" ]; then
        echo "[CHIKENGUN 2.5] ОШИБКА: не найдена графическая сессия (переменные DISPLAY и WAYLAND_DISPLAY пусты)."
        echo "  Запустите игру из графического терминала рабочего стола или укажите DISPLAY=:0 ./run.sh"
        exit 1
    fi
    if [ -x "$TARGET_BIN" ] && command -v nm >/dev/null 2>&1; then
        if nm -g --defined-only "$TARGET_BIN" 2>/dev/null | grep -qE ' T _?SDL_Init$'; then
            has_x11=0; has_wl=0
            nm -g --defined-only "$TARGET_BIN" 2>/dev/null | grep -qE ' [A-Za-z] _?X11_bootstrap$' && has_x11=1
            nm -g --defined-only "$TARGET_BIN" 2>/dev/null | grep -qE ' [A-Za-z] _?Wayland_bootstrap$' && has_wl=1
            if [ "${SDL_VIDEODRIVER:-}" = "wayland" ] && [ "$has_wl" -eq 0 ] && [ "$has_x11" -eq 1 ] && [ -n "${DISPLAY:-}" ]; then
                echo "[CHIKENGUN 2.5] SDL собран без Wayland-драйвера — переключаю SDL_VIDEODRIVER=x11 (через XWayland)."
                export SDL_VIDEODRIVER=x11
            elif [ -z "${SDL_VIDEODRIVER:-}" ] && [ -n "${WAYLAND_DISPLAY:-}" ] && [ -z "${DISPLAY:-}" ] && [ "$has_wl" -eq 1 ]; then
                export SDL_VIDEODRIVER=wayland
            fi
        fi
    fi
fi

cd "$(dirname "$TARGET_BIN")"
exec "$TARGET_BIN" ${APP_ARGS[@]+"${APP_ARGS[@]}"}
