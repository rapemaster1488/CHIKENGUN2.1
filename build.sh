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
# ВАЖНО: distro-пакет libbullet-dev (Bullet 2.x) НЕ СОВМЕСТИМ с заголовками Urho3D —
# Urho собран против Bullet 3 (libBullet3Collision), и компиляция падает с ошибкой
# "isVisible ... marked override, but does not override". Поэтому заголовки ищем
# ТОЛЬКО внутри самого Urho3D (ThirdParty/Bullet/src). Если не найдём — просто
# ругаемся подсказкой, но НЕ добавляем несовместимый include (ошибка всё равно не лечится).
BULLET_CFLAGS=""
for d in "$URHO_INC/Urho3D/ThirdParty/Bullet/src" "$HOME/urho3d/src/ThirdParty/Bullet/src" "$HOME/urho3d/ThirdParty/Bullet/src"; do
    if [ -f "$d/Bullet/LinearMath/btIDebugDraw.h" ] || [ -f "$d/LinearMath/btIDebugDraw.h" ]; then
        BULLET_CFLAGS="-I$d"
        echo "Bullet includes (из Urho3D): $d"
        break
    fi
done
if [ -z "$BULLET_CFLAGS" ]; then
    echo "Подсказка: если компилятор упадёт на Bullet/*.h — пересобери Urho3D из исходников"
    echo "(его ThirdParty/Bullet подставит нужные заголовки автоматически)."
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
    $BULLET_CFLAGS \
    -L"$URHO_LIBDIR" \
    -lUrho3D \
    $( [ -f "$URHO_LIBDIR/libBullet3Collision.so" ] || ls "$URHO_LIBDIR"/libBullet3Collision.so* >/dev/null 2>&1 && echo "-lBullet3Collision -lBullet3Dynamics -lBullet3Geometry -lBulletInverseDynamics" ) \
    $SDL_LIBS \
    -lGL \
    -lX11 \
    -lpthread \
    -lrt \
    -ldl \
    -lm

echo "Готово: $OUT"

# ---- 4.5 Автосинхронизация ресурсов в build/Data (чинит 'Could not find resource' и segfault) ----
# Игра запускается из build/ и грузит ONLY build/Data/. Если там неполная копия — докатываем
# недостающие файлы из ./Data и из ~/urho3d/bin/{Data,CoreData}, не перезаписывая имеющиеся.
sync_dir() {
    local src="$1" dst="build/Data"
    [ -d "$src" ] || return 0
    mkdir -p "$dst"
    cp -rn "$src"/. "$dst"/ 2>/dev/null || true
}
for base in "Data" "bin/Data" "$HOME/urho3d/bin/Data" "/usr/local/share/Urho3D/Data"; do
    sync_dir "$base"
done
# CoreData кладём и в build/CoreData (путь, который регистрирует движок), и в build/Data/CoreData
for cd_src in "CoreData" "Data/CoreData" "bin/CoreData" "$HOME/urho3d/bin/CoreData" "/usr/local/share/Urho3D/CoreData"; do
    if [ -d "$cd_src" ]; then
        mkdir -p build/CoreData build/Data/CoreData
        cp -rn "$cd_src"/. build/CoreData/ 2>/dev/null || true
        cp -rn "$cd_src"/. build/Data/CoreData/ 2>/dev/null || true
        break
    fi
done
if [ -d "build/Data/Materials/Game" ]; then
    echo "Ресурсы синхронизированы в build/Data (Materials/Game на месте)."
else
    echo ""
    echo "!! ВНИМАНИЕ: Materials/Game не найдены ни в ./Data, ни в ~/urho3d/bin/Data."
    echo "   Скопируй полный комплект ресурсов проекта (папка Data репо) рядом с проектом и повтори ./build.sh"
fi

# ---- 4.6 HD-текстуры: генерируем/синхронизируем TexturesHD -> build/Data/Textures ----
# Материалы Materials/Game/*.xml ссылаются на Textures/StoneHDDiffuse.png и т.д.
# Если PNG нет — генерируем процедурно (stdlib python3), потом копим в build/Data/Textures.
if [ ! -f "bin/Data/TexturesHD/StoneHDDiffuse.png" ] && [ ! -f "Data/TexturesHD/StoneHDDiffuse.png" ]; then
    if command -v python3 >/dev/null 2>&1 && [ -f tools/gen_textures.py ]; then
        echo "Генерирую HD-текстуры (tools/gen_textures.py -> bin/Data/TexturesHD)..."
        mkdir -p bin/Data/TexturesHD
        python3 tools/gen_textures.py bin/Data/TexturesHD || echo "!! Генерация текстур упала — сцена будет без HD-текстур"
    fi
fi
for hd in "bin/Data/TexturesHD" "Data/TexturesHD"; do
    if [ -d "$hd" ]; then
        mkdir -p build/Data/Textures
        cp -n "$hd"/*.png "$hd"/*.bmp build/Data/Textures/ 2>/dev/null || true
        echo "HD-текстуры синхронизированы: $hd -> build/Data/Textures/"
        break
    fi
done
