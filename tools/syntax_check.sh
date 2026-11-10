#!/usr/bin/env bash
# Быстрая проверка C++17 синтаксиса и типов Urho3D без линковки (~1–2 с).
# Использование:
#   ./tools/syntax_check.sh
#   URHO3D_HOME=/путь/к/urho3d ./tools/syntax_check.sh
#   URHO_INC=/путь/к/include   ./tools/syntax_check.sh
set -eo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CXX="${CXX:-g++}"

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
    "$HOME/urho3d/build/include"
    "$HOME/urho3d_build/include"
    "$HOME/urho3d-local/include"
    "/usr/local/include/Urho3D"
    "/usr/include/Urho3D"
)

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
if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists sdl2; then
    for f in $(pkg-config --cflags-only-I sdl2 | tr ' ' '\n' | sed 's/^-I//'); do
        [ -n "$f" ] && SDL_CFLAGS="$SDL_CFLAGS -isystem $f"
    done
fi
for d in "$FOUND_INC/Urho3D/ThirdParty" /usr/include /usr/local/include "$HOME/urho3d/build/include"; do
    [ -d "$d/SDL" ] && SDL_CFLAGS="$SDL_CFLAGS -isystem $d"
done
if ! echo "$SDL_CFLAGS" | grep -q "." && [ -d /usr/include/SDL2 ]; then
    mkdir -p /tmp/chikengun_sdl_shim
    ln -sfn /usr/include/SDL2 /tmp/chikengun_sdl_shim/SDL
    SDL_CFLAGS="-isystem /tmp/chikengun_sdl_shim"
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
