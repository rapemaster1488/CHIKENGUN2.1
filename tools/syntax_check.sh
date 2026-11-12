#!/usr/bin/env bash
# Быстрая проверка C++17 синтаксиса и типов Urho3D без линковки (~1–2 с).
# Работает на Linux, NixOS, Alpine, macOS и Windows MSYS2.
# Использование:
#   ./tools/syntax_check.sh
#   URHO3D_HOME=/путь/к/urho3d ./tools/syntax_check.sh
#   URHO_INC=/путь/к/include   ./tools/syntax_check.sh
set -eo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CXX="${CXX:-}"
if [ -z "$CXX" ]; then
    command -v g++ >/dev/null 2>&1 && CXX=g++ || CXX=clang++
fi

echo "[syntax] Проверка tools/gen_textures_4k.cpp..."
"$CXX" -std=c++17 -Wall -Wextra -fsyntax-only "$ROOT/tools/gen_textures_4k.cpp"
echo "[syntax] OK: tools/gen_textures_4k.cpp"

if [ -f "$ROOT/tests/test_core.cpp" ]; then
    echo "[syntax] Проверка tests/test_core.cpp..."
    "$CXX" -std=c++17 -Wall -Wextra -fsyntax-only "$ROOT/tests/test_core.cpp"
    echo "[syntax] OK: tests/test_core.cpp"
fi

CANDIDATES=()
[ -n "${URHO_INC:-}" ]    && CANDIDATES+=("$URHO_INC")
[ -n "${URHO3D_HOME:-}" ] && CANDIDATES+=("$URHO3D_HOME/include" "$URHO3D_HOME/build/include")
CANDIDATES+=(
    "$ROOT/.cache/urho3d-sdk/include"
    "$HOME/urho3d/build/include"
    "$HOME/urho3d-build/out/include"
    "$HOME/urho3d_build/include"
    "$HOME/urho3d-local/include"
    "/opt/homebrew/include/Urho3D"
    "/usr/local/include/Urho3D"
    "/usr/include/Urho3D"
)

if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists Urho3D 2>/dev/null; then
    PC_INC="$(pkg-config --variable=includedir Urho3D 2>/dev/null || true)"
    [ -n "$PC_INC" ] && CANDIDATES=("$PC_INC" "${CANDIDATES[@]}")
fi

FOUND_INC=""
for c in "${CANDIDATES[@]}"; do
    [ -z "$c" ] && continue
    if [ -f "$c/Urho3D/Urho3D.h" ] || [ -f "$c/Urho3D/Engine/Application.h" ]; then
        FOUND_INC="$c"; break
    fi
    if [ -f "$c/Core/UrhoVersion.h" ] || [ -f "$c/Urho3D.h" ] || [ -f "$c/Engine/Application.h" ]; then
        FOUND_INC="$(dirname "$c")"; break
    fi
done

if [ -z "$FOUND_INC" ]; then
    echo "[syntax] Внимание: заголовки Urho3D не найдены."
    echo "         Запустите: URHO3D_HOME=/путь/к/urho3d ./tools/syntax_check.sh"
    exit 0
fi

SDL_CFLAGS=""
SDL_SHIM_DIR=""
if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists sdl2 2>/dev/null; then
    for f in $(pkg-config --cflags-only-I sdl2 | tr ' ' '\n' | sed 's/^-I//'); do
        [ -n "$f" ] && SDL_CFLAGS="$SDL_CFLAGS -isystem $f"
        if [ -f "$f/SDL_joystick.h" ] && [ "$(basename "$f")" = "SDL2" ]; then
            SDL_SHIM_DIR="$f"
        elif [ -f "$f/SDL2/SDL_joystick.h" ]; then
            SDL_SHIM_DIR="$f/SDL2"
        fi
    done
fi
for d in "$FOUND_INC/Urho3D/ThirdParty" /opt/homebrew/include /usr/include /usr/local/include "$HOME/urho3d/build/include"; do
    if [ -d "$d/SDL" ]; then
        SDL_CFLAGS="$SDL_CFLAGS -isystem $d"
        SDL_SHIM_DIR=""
        break
    fi
    [ -z "$SDL_SHIM_DIR" ] && [ -d "$d/SDL2" ] && SDL_SHIM_DIR="$d/SDL2"
done
if [ -n "$SDL_SHIM_DIR" ] && [ ! -d "$FOUND_INC/Urho3D/ThirdParty/SDL" ]; then
    mkdir -p /tmp/chikengun_sdl_shim
    ln -sfn "$SDL_SHIM_DIR" /tmp/chikengun_sdl_shim/SDL
    SDL_CFLAGS="$SDL_CFLAGS -isystem /tmp/chikengun_sdl_shim"
fi

if [ -f "$FOUND_INC/Urho3D/ThirdParty/SDL/SDL_joystick.h" ] && [ ! -f "$FOUND_INC/Urho3D/ThirdParty/SDL/SDL_config.h" ]; then
    found_inc_parent="$(dirname "$FOUND_INC")"
    for scd in \
        "$found_inc_parent/Source/ThirdParty/SDL/include/generated" \
        "$found_inc_parent/Source/ThirdParty/SDL/include" \
        "/usr/include/SDL2" \
        "/usr/local/include/SDL2"
    do
        if [ -f "$scd/SDL_config.h" ]; then
            SDL_CFLAGS="$SDL_CFLAGS -isystem $scd"
            break
        fi
    done
fi

EXTRA=""
for d in \
    "$FOUND_INC/Urho3D/ThirdParty/Bullet" \
    "$FOUND_INC/Urho3D/ThirdParty/Bullet/src" \
    "$HOME/urho3d/src/ThirdParty/Bullet/src" \
    "$HOME/urho3d/ThirdParty/Bullet/src"
do
    if [ -f "$d/Bullet/LinearMath/btIDebugDraw.h" ] || [ -f "$d/LinearMath/btIDebugDraw.h" ]; then
        EXTRA="-isystem $d"; break
    fi
done

echo "[syntax] Проверка src/MyApp.cpp против заголовков Urho3D ($FOUND_INC)..."
"$CXX" -std=c++17 -Wall -Wextra -Werror -fsyntax-only \
    -isystem "$FOUND_INC" \
    -isystem "$FOUND_INC/Urho3D/ThirdParty" \
    $SDL_CFLAGS $EXTRA \
    "$ROOT/src/MyApp.cpp"
echo "[syntax] OK: src/MyApp.cpp (полная проверка типов Urho3D пройдена без предупреждений!)"
