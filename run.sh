#!/usr/bin/env bash
# ============================================================================
# Единый кросс-платформенный запуск CHIKENGUN 2.5 «ПЕРО СУДЬБЫ» (Native C++17)
# Работает на NixOS / Nix (flakes и nix-shell), Linux, macOS и Windows MSYS2.
#
# Флаги:
#   --install-deps   Поставить системные пакеты / войти в Nix devShell
#   --4k             Пересобрать PBR текстуры в 4096x4096
#   --rebuild        Принудительно пересобрать клиент
# ============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
ORIG_ARGS=("$@")
cd "$ROOT"

OS_NAME="$(uname -s)"
EXE_EXT=""
case "$OS_NAME" in
    MINGW*|MSYS*|CYGWIN*) EXE_EXT=".exe" ;;
esac

# На Nix/NixOS вне devShell сразу входим в nix develop / nix-shell, чтобы подхватились компилятор и OpenGL
if { [ -f /etc/NIXOS ] || [ -n "${NIX_CC:-}" ]; } && [ -z "${IN_NIX_SHELL:-}" ]; then
    if command -v nix >/dev/null 2>&1 && nix --extra-experimental-features "nix-command flakes" flake metadata "$ROOT" >/dev/null 2>&1; then
        echo "[NixOS] Запускаю внутри nix develop..."
        exec nix --extra-experimental-features "nix-command flakes" develop "$ROOT" -c "$0" "${ORIG_ARGS[@]}"
    elif command -v nix-shell >/dev/null 2>&1 && [ -f "$ROOT/shell.nix" ]; then
        echo "[NixOS] Запускаю внутри nix-shell..."
        exec nix-shell "$ROOT/shell.nix" --run "$(printf '%q ' "$0" "${ORIG_ARGS[@]}")"
    fi
fi

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

TARGET_BIN="$ROOT/build/MyShooter${EXE_EXT}"
if [ ! -x "$TARGET_BIN" ] && [ -x "$ROOT/build/Release/MyShooter${EXE_EXT}" ]; then
    TARGET_BIN="$ROOT/build/Release/MyShooter${EXE_EXT}"
fi

NEED_BUILD="$FORCE_BUILD"
if [ ! -x "$TARGET_BIN" ]; then
    NEED_BUILD=1
else
    for f in "$ROOT/src/"*; do
        if [ "$f" -nt "$TARGET_BIN" ]; then
            NEED_BUILD=1
            break
        fi
    done
fi

if [ "$NEED_BUILD" -eq 1 ]; then
    echo "[CHIKENGUN 2.5] Сборка / обновление нативного клиента..."
    "$ROOT/build.sh" "${BUILD_ARGS[@]}"
    TARGET_BIN="$ROOT/build/MyShooter${EXE_EXT}"
fi

if [ "$OS_NAME" = "Linux" ]; then
    # Драйверы OpenGL/Vulkan на NixOS живут в /run/opengl-driver/lib
    for ld in \
        "/run/opengl-driver/lib" \
        "/run/opengl-driver-32/lib" \
        "$ROOT/.cache/urho3d-sdk/lib" \
        "$ROOT/.cache/urho3d-sdk/lib64" \
        "${URHO3D_HOME:-}/lib" \
        "${URHO3D_HOME:-}/lib64" \
        "${URHO3D_HOME:-}/build/lib" \
        "$HOME/urho3d/build/lib" \
        "$HOME/urho3d_build/lib"
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

cd "$(dirname "$TARGET_BIN")"
exec "$TARGET_BIN" "${APP_ARGS[@]}"
