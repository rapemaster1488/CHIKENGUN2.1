#!/bin/bash
set -e

SRC="src/MyApp.cpp"
OUT="build/MyShooter"
mkdir -p build

# ---- 1. Ищем заголовки Urho3D (несколько типовых мест) ----
CANDIDATES=(
    "$HOME/urho3d/build/include"
    "/usr/local/include/Urho3D"
    "/usr/include/Urho3D"
)
URHO_INC=""
for c in "${CANDIDATES[@]}"; do
    if [ -f "$c/Core/UrhoVersion.h" ]; then URHO_INC="$c"; break; fi
    # вариант: сама директория уже содержит папку Urho3D/...
    if [ -f "$c/Urho3D.h" ] || [ -d "$c/Urho3D" ]; then
        # /usr/local/include/Urho3D содержит подпапки напрямую -> родитель = /usr/local/include
        URHO_INC="$(dirname "$c")"
        break
    fi
done
if [ -z "$URHO_INC" ]; then
    echo "Не найдены заголовки Urho3D. Укажи путь вручную: URHO_INC=/путь/к/include ./build.sh"
    exit 1
fi
echo "Urho3D includes: $URHO_INC"

# ---- 2. SDL2 через pkg-config (чинит fatal error: SDL/SDL_joystick.h) ----
if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists sdl2; then
    SDL_CFLAGS=$(pkg-config --cflags sdl2)
    SDL_LIBS=$(pkg-config --libs sdl2)
else
    SDL_CFLAGS="-I/usr/include/SDL2 -D_REENTRANT"
    SDL_LIBS="-lSDL2"
fi

# ---- 3. Путь к libUrho3D ----
if [ -d "$HOME/urho3d/build/lib" ]; then
    URHO_LIBDIR="$HOME/urho3d/build/lib"
elif [ -d "/usr/local/lib" ]; then
    URHO_LIBDIR="/usr/local/lib"
else
    URHO_LIBDIR="/usr/lib"
fi

CXX=${CXX:-g++}

$CXX "$SRC" \
    -o "$OUT" \
    -std=c++17 \
    -I"$URHO_INC" \
    -I"$URHO_INC/Urho3D/ThirdParty" \
    $SDL_CFLAGS \
    -L"$URHO_LIBDIR" \
    -lUrho3D \
    $SDL_LIBS \
    -lGL \
    -lX11 \
    -lpthread \
    -lrt \
    -ldl \
    -lm

echo "Готово: $OUT"
