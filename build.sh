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

# ---- 2. SDL2 (чинит fatal error: SDL/SDL_joystick.h) ----
# Urho3D делает #include <SDL/SDL_joystick.h>, а distro кладут заголовки в /usr/include/SDL2.
# Варианты: pkg-config дал путь .../include/SDL2, либо симлинк SDL -> SDL2, либо fallback.
SDL_INC=""
if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists sdl2; then
    SDL_LIBS=$(pkg-config --libs sdl2)
    for f in $(pkg-config --cflags-only-I sdl2 | tr ' ' '\n' | sed 's/^-I//'); do
        if [ -f "$f/SDL/SDL_joystick.h" ]; then SDL_INC="$f"; break; fi
        if [ -f "$f/SDL2/SDL_joystick.h" ]; then SDL_INC2="$f"; fi
    done
else
    SDL_LIBS="-lSDL2"
    if [ -f /usr/include/SDL/SDL_joystick.h ]; then SDL_INC="/usr/include"; fi
    if [ -f /usr/include/SDL2/SDL_joystick.h ]; then SDL_INC2="/usr/include"; fi
fi

if [ -z "$SDL_INC" ] && [ -n "$SDL_INC2" ]; then
    # нет пути SDL/... — пробуем создать симлинк SDL2 -> SDL (нужен root или права на include)
    if [ ! -e "$SDL_INC2/SDL" ] && ln -s "$SDL_INC2/SDL2" "$SDL_INC2/SDL" 2>/dev/null; then
        echo "Создан симлинк: $SDL_INC2/SDL -> $SDL_INC2/SDL2"
    fi
    if [ -f "$SDL_INC2/SDL/SDL_joystick.h" ]; then
        SDL_INC="$SDL_INC2"
    else
        echo "!! Не найден заголовок SDL/SDL_joystick.h."
        echo "   Выполни одну команду и повтори сборку:"
        echo "   sudo apt install -y libsdl2-dev && sudo ln -sfn /usr/include/SDL2 /usr/include/SDL"
        exit 1
    fi
fi

SDL_CFLAGS="-D_REENTRANT"
[ -n "$SDL_INC" ] && SDL_CFLAGS="-I$SDL_INC $SDL_CFLAGS"

# ---- 2.5 Bullet Physics (чинит fatal error: Bullet/LinearMath/btIDebugDraw.h) ----
# Urho3D делает #include <Bullet/...>, а distro кладут заголовки в /usr/include/bullet.
BULLET_INC=""
if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists bullet; then
    for f in $(pkg-config --cflags-only-I bullet | tr ' ' '\n' | sed 's/^-I//'); do
        if [ -f "$f/Bullet/LinearMath/btIDebugDraw.h" ]; then BULLET_INC="$f"; break; fi
        if [ -f "$f/bullet/LinearMath/btIDebugDraw.h" ]; then BULLET_INC2_PARENT="$f"; fi
    done
fi
if [ -z "$BULLET_INC" ]; then
    for d in /usr/include /usr/local/include; do
        if [ -f "$d/Bullet/LinearMath/btIDebugDraw.h" ]; then BULLET_INC="$d"; break; fi
        if [ -f "$d/bullet/LinearMath/btIDebugDraw.h" ]; then BULLET_INC2_PARENT="$d"; fi
    done
fi
if [ -z "$BULLET_INC" ] && [ -n "$BULLET_INC2_PARENT" ]; then
    # нет пути Bullet/... — создаём симлинк bullet -> Bullet (или наоборот)
    if [ ! -e "$BULLET_INC2_PARENT/Bullet" ]; then
        if ln -s "$BULLET_INC2_PARENT/bullet" "$BULLET_INC2_PARENT/Bullet" 2>/dev/null; then
            echo "Создан симлинк: $BULLET_INC2_PARENT/Bullet -> $BULLET_INC2_PARENT/bullet"
        fi
    fi
    [ -f "$BULLET_INC2_PARENT/Bullet/LinearMath/btIDebugDraw.h" ] && BULLET_INC="$BULLET_INC2_PARENT"
fi
if [ -z "$BULLET_INC" ]; then
    echo "!! Не найден заголовок Bullet/LinearMath/btIDebugDraw.h."
    echo "   Выполни одну команду и повтори сборку:"
    echo "   sudo apt install -y libbullet-dev && sudo ln -sfn /usr/include/bullet /usr/include/Bullet"
    exit 1
fi
BULLET_CFLAGS="-I$BULLET_INC"
echo "Bullet includes: $BULLET_INC"

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
    $BULLET_CFLAGS \
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
