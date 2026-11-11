#!/usr/bin/env bash
# ============================================================================
# Сборка CHIKENGUN 2.3 — 4K ULTRA & CYBER EVOLUTION EDITION (Urho3D C++17)
# Автоматически находит Urho3D / SDL2 / Bullet3, синхронизирует шейдеры,
# PBR-материалы и звуки, генерирует 4K PBR текстуры (4096x4096) + 360° Skybox4K
# + 4K Loading Art и компилирует build/MyShooter.
# Переменная TEX_SIZE (по умолчанию 4096) управляет разрешением генератора.
# ============================================================================
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--O2 -DNDEBUG -Wall -Wextra}"
TEX_SIZE="${TEX_SIZE:-4096}"
OUT="$ROOT/build/MyShooter"

mkdir -p "$ROOT/build"

# ---- 1. Автопоиск заголовков Urho3D ----
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
        FOUND_INC="$c"
        break
    fi
    if [ -f "$c/Core/UrhoVersion.h" ] || [ -f "$c/Urho3D.h" ] || [ -f "$c/Engine/Application.h" ]; then
        FOUND_INC="$(dirname "$c")"
        break
    fi
done

if [ -z "$FOUND_INC" ]; then
    echo "ОШИБКА: Не найдены заголовки Urho3D."
    echo "Укажите путь вручную: URHO3D_HOME=/путь/к/urho3d ./build.sh  или  URHO_INC=/путь/к/include ./build.sh"
    exit 1
fi
URHO_INC="$FOUND_INC"
echo "[Urho3D] Заголовки: $URHO_INC"

# ---- 2. SDL2 (без sudo: локальный shim build/sdl_shim/SDL -> .../SDL2 при необходимости) ----
SDL_INC=""
SDL_INC2=""
if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists sdl2; then
    SDL_LIBS="$(pkg-config --libs sdl2)"
    for f in $(pkg-config --cflags-only-I sdl2 | tr ' ' '\n' | sed 's/^-I//'); do
        if [ -f "$f/SDL/SDL_joystick.h" ]; then SDL_INC="$f"; break; fi
        if [ -f "$f/SDL2/SDL_joystick.h" ]; then SDL_INC2="$f/SDL2"; fi
        if [ -f "$f/SDL_joystick.h" ] && [ "$(basename "$f")" = "SDL2" ]; then SDL_INC2="$f"; fi
    done
else
    SDL_LIBS="-lSDL2"
fi

if [ -z "$SDL_INC" ]; then
    for d in "$URHO_INC/Urho3D/ThirdParty" "/usr/include" "/usr/local/include"; do
        if [ -f "$d/SDL/SDL_joystick.h" ]; then SDL_INC="$d"; break; fi
        if [ -z "$SDL_INC2" ] && [ -f "$d/SDL2/SDL_joystick.h" ]; then SDL_INC2="$d/SDL2"; fi
    done
fi

if [ -z "$SDL_INC" ] && [ -n "$SDL_INC2" ]; then
    mkdir -p "$ROOT/build/sdl_shim"
    ln -sfn "$SDL_INC2" "$ROOT/build/sdl_shim/SDL"
    SDL_INC="$ROOT/build/sdl_shim"
fi

SDL_CFLAGS="-D_REENTRANT"
[ -n "$SDL_INC" ] && SDL_CFLAGS="-isystem $SDL_INC $SDL_CFLAGS"

# ---- 3. Bullet3 из дерева Urho3D (если доступен) ----
BULLET_CFLAGS=""
for d in \
    "$URHO_INC/Urho3D/ThirdParty/Bullet" \
    "$URHO_INC/Urho3D/ThirdParty/Bullet/src" \
    "${URHO3D_HOME:-}/include/Urho3D/ThirdParty/Bullet" \
    "$HOME/urho3d/src/ThirdParty/Bullet/src" \
    "$HOME/urho3d/ThirdParty/Bullet/src"
do
    [ -z "$d" ] && continue
    if [ -f "$d/Bullet/LinearMath/btIDebugDraw.h" ] || [ -f "$d/LinearMath/btIDebugDraw.h" ]; then
        BULLET_CFLAGS="-isystem $d"
        break
    fi
done

# ---- 4. Автопоиск libUrho3D ----
URHO_LIBDIR=""
for ld in \
    "${URHO3D_HOME:-}/lib" \
    "${URHO3D_HOME:-}/build/lib" \
    "$(dirname "$URHO_INC")/lib" \
    "$HOME/urho3d/build/lib" \
    "$HOME/urho3d_build/lib" \
    "/usr/local/lib" \
    "/usr/lib/x86_64-linux-gnu" \
    "/usr/lib"
do
    [ -z "$ld" ] && continue
    if [ -f "$ld/libUrho3D.so" ] || [ -f "$ld/libUrho3D.a" ]; then
        URHO_LIBDIR="$ld"
        break
    fi
done

if [ -z "$URHO_LIBDIR" ]; then
    echo "ОШИБКА: Не найдена библиотека libUrho3D (.so/.a)."
    echo "Укажите путь: URHO3D_HOME=/путь/к/urho3d ./build.sh"
    exit 1
fi
echo "[Urho3D] Библиотеки: $URHO_LIBDIR"

BULLET_LIBS=()
for lib in Bullet3Collision Bullet3Dynamics Bullet3Geometry BulletInverseDynamics; do
    if ls "$URHO_LIBDIR/lib$lib".so* "$URHO_LIBDIR/lib$lib".a >/dev/null 2>&1; then
        BULLET_LIBS+=("-l$lib")
    fi
done

# ---- 5. Синхронизация ресурсов (CoreData + Data + settings.ini) ----
echo "[1/3] Синхронизация GLSL шейдеров, 4K постпроцессов, PBR материалов и звуков..."
mkdir -p "$ROOT/build/CoreData" "$ROOT/build/Data"
for csrc in "$HOME/urho3d/bin/CoreData" "/usr/local/share/Urho3D/CoreData" "$ROOT/CoreData"; do
    [ -d "$csrc" ] && cp -rf "$csrc"/. "$ROOT/build/CoreData"/ 2>/dev/null || true
done
for dsrc in "$HOME/urho3d/bin/Data" "/usr/local/share/Urho3D/Data" "$ROOT/Data"; do
    [ -d "$dsrc" ] && cp -rf "$dsrc"/. "$ROOT/build/Data"/ 2>/dev/null || true
done

# Удаляем случайные вложенные дубли и пустые файлы (но НЕ трогаем внешнюю установку Urho3D!)
rm -rf "$ROOT/build/Data/Data" "$ROOT/build/Data/CoreData" 2>/dev/null || true
find "$ROOT/build/Data" -name "*.mat" ! -path "*Materials*" -delete 2>/dev/null || true
find "$ROOT/build/Data" "$ROOT/build/CoreData" -type f -size 0 -delete 2>/dev/null || true
[ -f "$ROOT/bin/settings.ini" ] && [ ! -f "$ROOT/build/settings.ini" ] && cp "$ROOT/bin/settings.ini" "$ROOT/build/settings.ini" || true

# ---- 6. Генерация полного пакета 4K PBR текстур (4096x4096) ----
STAMP_FILE="$ROOT/build/Data/Textures/.4k_pbr_stamp"
CUR_STAMP=""
[ -f "$STAMP_FILE" ] && CUR_STAMP="$(cat "$STAMP_FILE" 2>/dev/null || true)"

if [ "$CUR_STAMP" != "$TEX_SIZE" ] || [ ! -f "$ROOT/build/Data/Textures/FloorTileRoughMetal.png" ] || [ ! -f "$ROOT/build/Data/Textures/BanyaLogDiffuse.png" ] || [ ! -f "$ROOT/build/Data/Textures/BanyaRoosterDiffuse.png" ] || [ ! -f "$ROOT/build/Data/Textures/Skybox4K.xml" ]; then
    echo "[2/3] Генерация 4K PBR текстур (${TEX_SIZE}x${TEX_SIZE}), IBL Skybox4K и 4K Loading Art..."
    "$CXX" -O3 -std=c++17 -pthread "$ROOT/tools/gen_textures_4k.cpp" -o "$ROOT/build/gen_textures_4k"
    "$ROOT/build/gen_textures_4k" "$ROOT/build/Data/Textures" "$TEX_SIZE"
else
    echo "[2/3] 4K PBR текстуры (${TEX_SIZE}x${TEX_SIZE}) уже актуальны."
fi

# ---- 7. Компиляция игры ----
echo "[3/3] Компиляция src/MyApp.cpp -> build/MyShooter..."
"$CXX" "$ROOT/src/MyApp.cpp" \
    -o "$OUT" \
    -std=c++17 $CXXFLAGS \
    -isystem "$URHO_INC" \
    -isystem "$URHO_INC/Urho3D/ThirdParty" \
    $SDL_CFLAGS \
    $BULLET_CFLAGS \
    -L"$URHO_LIBDIR" \
    -Wl,-rpath,"$URHO_LIBDIR" \
    -lUrho3D \
    "${BULLET_LIBS[@]}" \
    $SDL_LIBS \
    -lGL -lX11 -lpthread -lrt -ldl -lm

echo "================================================================"
echo "Готово! Запускайте CHIKENGUN 2.3 [4K ULTRA EDITION]:"
echo "  ./run.sh   или   ./build/MyShooter"
echo "================================================================"
