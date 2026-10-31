#!/bin/bash
# Быстрая проверка синтаксиса src/MyApp.cpp БЕЗ сборки и линковки.
# ~1-2 секунды вместо полной сборки: ловит опечатки, неверные сигнатуры,
# отсутствующие методы — всё, что обычно выясняется только на ./build.sh.
#
# Использование:
#     tools/syntax_check.sh                 # автоопределение путей
#     URHO_INC=/usr/local/include tools/syntax_check.sh
#
# Важно: это НЕ заменяет сборку. Ошибки линковки (нет -lUrho3D и т.п.) здесь не видны.

set -o pipefail

SRC="src/MyApp.cpp"
CXX=${CXX:-g++}

if [ ! -f "$SRC" ]; then
    echo "Запусти из корня репозитория (нет $SRC)"
    exit 1
fi

# --- заголовки Urho3D (та же логика, что в build.sh) ---
if [ -z "$URHO_INC" ]; then
    for c in "$HOME/urho3d/build/include" "/usr/local/include/Urho3D" "/usr/include/Urho3D"; do
        if [ -f "$c/Core/UrhoVersion.h" ]; then URHO_INC="$c"; break; fi
        if [ -f "$c/Urho3D.h" ] || [ -d "$c/Urho3D" ]; then URHO_INC="$(dirname "$c")"; break; fi
    done
fi
if [ -z "$URHO_INC" ]; then
    echo "Не найдены заголовки Urho3D. Укажи: URHO_INC=/путь/к/include $0"
    exit 1
fi

# --- SDL2 (<SDL/...> против <SDL2/...>) ---
SDL_CFLAGS=""
if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists sdl2; then
    for f in $(pkg-config --cflags-only-I sdl2 | tr ' ' '\n' | sed 's/^-I//'); do
        [ -n "$f" ] && SDL_CFLAGS="$SDL_CFLAGS -I$f"
    done
fi
for d in /usr/include /usr/local/include "$HOME/urho3d/build/include"; do
    [ -d "$d/SDL" ] && SDL_CFLAGS="$SDL_CFLAGS -I$d"
done

EXTRA=""
for d in "$URHO_INC/Urho3D/ThirdParty/Bullet/src" "$HOME/urho3d/src/ThirdParty/Bullet/src" "$HOME/urho3d/ThirdParty/Bullet/src"; do
    if [ -f "$d/Bullet/LinearMath/btIDebugDraw.h" ] || [ -f "$d/LinearMath/btIDebugDraw.h" ]; then
        EXTRA="-I$d"; break
    fi
done

echo "Urho3D includes: $URHO_INC"
echo "Проверяю $SRC ..."

# -fsyntax-only: компилятор разбирает файл целиком, но ничего не генерирует
$CXX "$SRC" -fsyntax-only -std=c++17 \
    -I"$URHO_INC" \
    -I"$URHO_INC/Urho3D/ThirdParty" \
    $SDL_CFLAGS $EXTRA \
    ${CXXFLAGS:-}

rc=$?
if [ $rc -eq 0 ]; then
    echo "OK: синтаксис и типы в порядке"
else
    echo "Есть ошибки (код возврата $rc) — см. вывод выше"
fi
exit $rc
