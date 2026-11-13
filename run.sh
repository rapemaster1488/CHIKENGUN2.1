#!/usr/bin/env bash
# ============================================================================
# Единый кросс-платформенный запуск CHIKENGUN 2.5 «ПЕРО СУДЬБЫ» (Native C++17)
# Работает на NixOS / Nix (локальный профиль, nix-shell и flakes), Linux, macOS и Windows MSYS2.
#
# Флаги:
#   --install-deps   Поставить системные пакеты / войти в Nix shell
#   --4k             Пересобрать PBR текстуры в 4096x4096
#   --rebuild        Принудительно пересобрать клиент
#   --rebuild-urho3d Пересобрать кэшированный локальный Urho3D SDK и клиент
#   --diagnose-sdl   Проверить SDL-драйверы, дисплей, OpenGL и аудио (Linux)
#   -nosound         Запустить без звука
#   -j N / --jobs N  Ограничить число потоков при пересборке
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
REBUILD_URHO3D=0
DIAGNOSE_SDL=0
USER_URHO_HOME="${URHO3D_HOME:-}"
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
        --rebuild-urho3d)
            BUILD_ARGS+=("$1")
            FORCE_BUILD=1
            REBUILD_URHO3D=1
            shift
            ;;
        --diagnose-sdl)
            APP_ARGS+=("$1")
            DIAGNOSE_SDL=1
            shift
            ;;
        -j|--jobs)
            if [[ ! "${2:-}" =~ ^[1-9][0-9]*$ ]]; then
                echo "ОШИБКА: $1 требует положительное число потоков." >&2
                exit 2
            fi
            BUILD_ARGS+=("$1" "$2")
            shift 2
            ;;
        -j?*|--jobs=*)
            JOBS_VALUE="${1#-j}"
            if [[ "$1" == --jobs=* ]]; then JOBS_VALUE="${1#--jobs=}"; fi
            if [[ ! "$JOBS_VALUE" =~ ^[1-9][0-9]*$ ]]; then
                echo "ОШИБКА: $1 требует положительное число потоков." >&2
                exit 2
            fi
            BUILD_ARGS+=("$1")
            shift
            ;;
        -h|--help)
            sed -n '2,14p' "$0" | sed 's/^# \?//'
            exit 0
            ;;
        *)
            APP_ARGS+=("$1")
            shift
            ;;
    esac
done

# A remembered external SDK must not prevent an explicit local SDK rebuild.
if [ -z "${URHO3D_HOME:-}" ] && [ "$REBUILD_URHO3D" -eq 0 ] && [ -f "$ROOT/build/.urho_sdk_home" ]; then
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
        # Read all of nm's output: grep -q with pipefail can SIGPIPE nm.
        # Only a positively identified static, bundled SDL can be checked here.
        if nm -g --defined-only "$bin" 2>/dev/null | grep -E ' [TW] _?SDL_Init$' >/dev/null; then
            if ! nm -g --defined-only "$bin" 2>/dev/null | grep -E ' [A-Za-z] _?(X11_bootstrap|Wayland_bootstrap|KMSDRM_bootstrap)$' >/dev/null; then
                return 0
            fi
        fi
    fi
    return 1
}

NEED_BUILD="$FORCE_BUILD"
if [ ! -x "$TARGET_BIN" ] || [ ! -f "$ROOT/build/Data/Textures/Skybox4K.xml" ] || [ ! -f "$ROOT/build/Data/Textures/BanyaBirchDiffuse.png" ] || [ ! -d "$ROOT/build/CoreData/Shaders" ]; then
    NEED_BUILD=1
elif [ "$DIAGNOSE_SDL" -eq 0 ] && bin_missing_video_driver "$TARGET_BIN"; then
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

# Keep the declared order: an explicitly selected SDK must not be shadowed by
# another copy with the same shared-library name in a cache or home directory.
RUNTIME_PATHS=""
append_runtime_dir() {
    local dir="$1"
    if [ -n "$dir" ] && [ -d "$dir" ]; then
        RUNTIME_PATHS="${RUNTIME_PATHS:+$RUNTIME_PATHS:}$dir"
    fi
}
# Prefer the exact library directory recorded by the successful build, unless
# the user has since explicitly selected a different SDK.
if [ -f "$ROOT/build/.urho_libdir" ]; then
    saved_libdir="$(cat "$ROOT/build/.urho_libdir" 2>/dev/null || true)"
    if [ -z "$USER_URHO_HOME" ]; then
        append_runtime_dir "$saved_libdir"
    else
        user_home_real="$(cd "$USER_URHO_HOME" 2>/dev/null && pwd -P || true)"
        if [ -n "$user_home_real" ]; then
            case "$saved_libdir/" in "$user_home_real"/*) append_runtime_dir "$saved_libdir" ;; esac
        fi
    fi
fi
if [ "$OS_NAME" = "Linux" ]; then
    # Драйверы OpenGL/Vulkan на NixOS живут в /run/opengl-driver/lib + локальные профили Nix
    for ld in \
        "${URHO3D_HOME:+$URHO3D_HOME/lib}" \
        "${URHO3D_HOME:+$URHO3D_HOME/lib/Urho3D}" \
        "${URHO3D_HOME:+$URHO3D_HOME/lib64}" \
        "${URHO3D_HOME:+$URHO3D_HOME/lib64/Urho3D}" \
        "${URHO3D_HOME:+$URHO3D_HOME/build/lib}" \
        "${URHO3D_HOME:+$URHO3D_HOME/build/lib/Urho3D}"
    do
        append_runtime_dir "$ld"
    done
    if [ -n "${URHO3D_HOME:-}" ]; then
        for ld in "$URHO3D_HOME"/lib/*-linux-* "$URHO3D_HOME"/lib/*-linux-*/Urho3D; do
            append_runtime_dir "$ld"
        done
    fi
    for ld in \
        "$ROOT/.cache/urho3d-sdk/lib" \
        "$ROOT/.cache/urho3d-sdk/lib/Urho3D" \
        "$ROOT/.cache/urho3d-sdk/lib64" \
        "$ROOT/.cache/urho3d-sdk/lib64/Urho3D" \
        "/run/opengl-driver/lib" \
        "/run/opengl-driver-32/lib" \
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
        append_runtime_dir "$ld"
    done
    if [ -n "$RUNTIME_PATHS" ]; then
        export LD_LIBRARY_PATH="${RUNTIME_PATHS}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    fi
elif [ "$OS_NAME" = "Darwin" ]; then
    for ld in \
        "${URHO3D_HOME:+$URHO3D_HOME/lib}" \
        "${URHO3D_HOME:+$URHO3D_HOME/lib/Urho3D}" \
        "${URHO3D_HOME:+$URHO3D_HOME/build/lib}" \
        "${URHO3D_HOME:+$URHO3D_HOME/build/lib/Urho3D}" \
        "$ROOT/.cache/urho3d-sdk/lib" \
        "$ROOT/.cache/urho3d-sdk/lib/Urho3D" \
        "$HOME/urho3d/build/lib" \
        "/opt/homebrew/lib" \
        "/usr/local/lib"
    do
        append_runtime_dir "$ld"
    done
    if [ -n "$RUNTIME_PATHS" ]; then
        export DYLD_LIBRARY_PATH="${RUNTIME_PATHS}${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
    fi
fi

export CHIKENGUN_DATA_DIR="${CHIKENGUN_DATA_DIR:-$ROOT/build/Data}"
export CHIKENGUN_COREDATA_DIR="${CHIKENGUN_COREDATA_DIR:-$ROOT/build/CoreData}"

# The client performs SDL diagnostics, including when no display is present.
# Preserve any SDL_VIDEODRIVER explicitly selected by the user.

cd "$(dirname "$TARGET_BIN")"
exec "$TARGET_BIN" ${APP_ARGS[@]+"${APP_ARGS[@]}"}
