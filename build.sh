#!/usr/bin/env bash
# ============================================================================
# CHIKENGUN 2.5 — ПЕРО СУДЬБЫ (Zero-Pain Native Builder & Auto-Bootstrap)
# Автоматически:
#   1) проверяет компилятор, CMake, SDL2, OpenGL, X11, Git, Python 3
#      (при --install-deps или в интерактивном терминале ставит их одной командой);
#   2) ищет установленный Urho3D SDK или один раз собирает минимальный
#      статический Urho3D в .cache/urho3d-sdk (автономный build/MyShooter);
#   3) синтезирует звуки, проверяет и генерирует все 9 PBR-наборов текстур
#      (в т.ч. BanyaLog / BanyaRooster), синхронизирует Data/ и CoreData/;
#   4) собирает build/MyShooter.
#
# Флаги:
#   --install-deps   Автоматически установить системные пакеты сборки (apt/dnf/pacman/zypper/brew)
#   --4k             Сгенерировать текстуры максимального разрешения 4096x4096 (по умолчанию 1024)
#   --tests-only     Собрать и прогнать только модульные C++17 тесты (без Urho3D)
#   -j N / --jobs N  Число потоков сборки (по умолчанию все ядра)
# ============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--O2 -DNDEBUG -Wall -Wextra}"
TEX_SIZE="${TEX_SIZE:-1024}"
URHO3D_REF="${URHO3D_REF:-1.9.0}"
INSTALL_DEPS="${AUTO_INSTALL_DEPS:-0}"
TESTS_ONLY=0
JOBS=""
OUT="$ROOT/build/MyShooter"

while [ "$#" -gt 0 ]; do
    case "$1" in
        --install-deps) INSTALL_DEPS=1; shift ;;
        --4k)           TEX_SIZE=4096; shift ;;
        --tests-only)   TESTS_ONLY=1; shift ;;
        -j|--jobs)      JOBS="${2:-}"; shift 2 ;;
        -h|--help)
            sed -n '2,18p' "$0" | sed 's/^# \?//'
            exit 0
            ;;
        *) shift ;;
    esac
done

if [ -z "$JOBS" ]; then
    if command -v nproc >/dev/null 2>&1; then
        JOBS="$(nproc)"
    elif command -v sysctl >/dev/null 2>&1; then
        JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
    else
        JOBS=4
    fi
fi

mkdir -p "$ROOT/build"

# ---- Быстрая ветка: только headless C++17 тесты ----
if [ "$TESTS_ONLY" -eq 1 ]; then
    echo "[Tests] Компиляция gen_textures_4k + test_core..."
    "$CXX" -O3 -std=c++17 -pthread "$ROOT/tools/gen_textures_4k.cpp" -o "$ROOT/build/gen_textures_4k"
    "$CXX" -O2 -std=c++17 -Wall -Wextra -Werror -UNDEBUG "$ROOT/tests/test_core.cpp" -o "$ROOT/build/test_core"
    "$ROOT/build/test_core" "$ROOT/build/gen_textures_4k"
    exit 0
fi

# ---- 1. Проверка и автоустановка системных зависимостей ----
detect_pkg_mgr() {
    if command -v apt-get >/dev/null 2>&1; then echo "apt"; return; fi
    if command -v dnf >/dev/null 2>&1; then echo "dnf"; return; fi
    if command -v pacman >/dev/null 2>&1; then echo "pacman"; return; fi
    if command -v zypper >/dev/null 2>&1; then echo "zypper"; return; fi
    if command -v brew >/dev/null 2>&1; then echo "brew"; return; fi
    echo "unknown"
}

check_missing_tools() {
    local missing=()
    command -v "$CXX" >/dev/null 2>&1 || command -v g++ >/dev/null 2>&1 || command -v clang++ >/dev/null 2>&1 || missing+=("c++-compiler")
    command -v cmake >/dev/null 2>&1 || missing+=("cmake")
    command -v make >/dev/null 2>&1 || missing+=("make")
    command -v git >/dev/null 2>&1 || missing+=("git")
    command -v python3 >/dev/null 2>&1 || missing+=("python3")

    if [ "$(uname -s)" = "Linux" ]; then
        local has_sdl=0 has_gl=0 has_x11=0
        if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists sdl2 2>/dev/null; then
            has_sdl=1
        elif [ -f /usr/include/SDL2/SDL.h ] || [ -f /usr/local/include/SDL2/SDL.h ]; then
            has_sdl=1
        fi
        [ -f /usr/include/GL/gl.h ] || [ -f /usr/local/include/GL/gl.h ] && has_gl=1
        [ -f /usr/include/X11/Xlib.h ] || [ -f /usr/local/include/X11/Xlib.h ] && has_x11=1
        [ "$has_sdl" -eq 0 ] && missing+=("sdl2-dev")
        [ "$has_gl" -eq 0 ] && missing+=("opengl-dev")
        [ "$has_x11" -eq 0 ] && missing+=("x11-dev")
    fi
    echo "${missing[*]:-}"
}

install_cmd_for_mgr() {
    case "$1" in
        apt)
            echo "sudo apt-get update && sudo apt-get install -y build-essential cmake git pkg-config python3 libsdl2-dev libgl-dev libglu1-mesa-dev libx11-dev libxrandr-dev libxi-dev libxext-dev libasound2-dev libpulse-dev libudev-dev"
            ;;
        dnf)
            echo "sudo dnf install -y gcc-c++ cmake make git pkgconf-pkg-config python3 SDL2-devel mesa-libGL-devel mesa-libGLU-devel libX11-devel libXrandr-devel libXi-devel alsa-lib-devel pulseaudio-libs-devel systemd-devel"
            ;;
        pacman)
            echo "sudo pacman -S --needed --noconfirm base-devel cmake git pkgconf python sdl2 mesa glu libx11 libxrandr libxi alsa-lib libpulse"
            ;;
        zypper)
            echo "sudo zypper install -y gcc-c++ cmake make git pkg-config python3 libSDL2-devel Mesa-libGL-devel libX11-devel libXrandr-devel alsa-devel"
            ;;
        brew)
            echo "brew install cmake sdl2 python"
            ;;
        *)
            echo ""
            ;;
    esac
}

MISSING="$(check_missing_tools)"
if [ -n "$MISSING" ]; then
    PKG_MGR="$(detect_pkg_mgr)"
    CMD="$(install_cmd_for_mgr "$PKG_MGR")"
    echo "[Зависимости] Не хватает: $MISSING"
    if [ "$INSTALL_DEPS" -eq 0 ] && [ -t 0 ] && [ -n "$CMD" ]; then
        printf "Установить системные пакеты автоматически? [Y/n]: "
        read -r ans || ans="n"
        case "${ans:-y}" in
            [YyДд]*|"") INSTALL_DEPS=1 ;;
        esac
    fi
    if [ "$INSTALL_DEPS" -eq 1 ] && [ -n "$CMD" ]; then
        echo "[Зависимости] Устанавливаю: $CMD"
        bash -c "$CMD"
    else
        echo "ОШИБКА: Установите недостающие зависимости и повторите запуск:"
        if [ -n "$CMD" ]; then
            echo "  $CMD"
        else
            echo "  Потребуются: C++17 компилятор, CMake, make, git, python3, SDL2, OpenGL, X11 dev-пакеты"
        fi
        echo "Или запустите: ./build.sh --install-deps"
        exit 1
    fi
fi

if ! command -v "$CXX" >/dev/null 2>&1; then
    command -v g++ >/dev/null 2>&1 && CXX=g++ || CXX=clang++
fi

# ---- 2. Автопоиск заголовков и библиотеки Urho3D (включая локальный кэш .cache/urho3d-sdk) ----
LOCAL_SDK="$ROOT/.cache/urho3d-sdk"
find_urho3d() {
    URHO_INC=""
    URHO_LIBDIR=""
    local candidates=()
    [ -n "${URHO_INC_OVERRIDE:-}" ] && candidates+=("$URHO_INC_OVERRIDE")
    [ -n "${URHO3D_HOME:-}" ]       && candidates+=("$URHO3D_HOME/include" "$URHO3D_HOME/build/include")
    candidates+=(
        "$LOCAL_SDK/include"
        "$HOME/urho3d/build/include"
        "$HOME/urho3d_build/include"
        "$HOME/urho3d-local/include"
        "/usr/local/include/Urho3D"
        "/usr/local/include"
        "/usr/include/Urho3D"
        "/usr/include"
    )
    for c in "${candidates[@]}"; do
        [ -z "$c" ] && continue
        if [ -f "$c/Urho3D/Engine/Application.h" ] || [ -f "$c/Urho3D/Urho3D.h" ]; then
            URHO_INC="$c"
            break
        fi
        if [ -f "$c/Engine/Application.h" ] || [ -f "$c/Core/UrhoVersion.h" ]; then
            URHO_INC="$(dirname "$c")"
            break
        fi
    done

    local libdirs=()
    [ -n "${URHO3D_HOME:-}" ] && libdirs+=("$URHO3D_HOME/lib" "$URHO3D_HOME/build/lib" "$URHO3D_HOME/lib/Urho3D")
    [ -n "$URHO_INC" ] && libdirs+=("$(dirname "$URHO_INC")/lib" "$(dirname "$URHO_INC")/lib/Urho3D")
    libdirs+=(
        "$LOCAL_SDK/lib"
        "$LOCAL_SDK/lib/Urho3D"
        "$HOME/urho3d/build/lib"
        "$HOME/urho3d_build/lib"
        "/usr/local/lib"
        "/usr/local/lib/Urho3D"
        "/usr/lib/x86_64-linux-gnu"
        "/usr/lib"
    )
    for ld in "${libdirs[@]}"; do
        [ -z "$ld" ] && continue
        if [ -f "$ld/libUrho3D.a" ] || [ -f "$ld/libUrho3D.so" ] || [ -f "$ld/libUrho3D.dylib" ]; then
            URHO_LIBDIR="$ld"
            break
        fi
    done
}

URHO_INC_OVERRIDE="${URHO_INC:-}"
find_urho3d

if [ -z "$URHO_INC" ] || [ -z "$URHO_LIBDIR" ]; then
    echo "================================================================"
    echo "[Urho3D] Локальный SDK не найден."
    echo "[Urho3D] Скачиваю и собираю минимальный статический Urho3D ($URHO3D_REF)"
    echo "         в $LOCAL_SDK (делается ОДИН раз, далее кэшируется)..."
    echo "================================================================"
    URHO_SRC="$ROOT/.cache/urho3d-src"
    URHO_BLD="$ROOT/.cache/urho3d-build"
    mkdir -p "$ROOT/.cache"
    if [ ! -d "$URHO_SRC/.git" ]; then
        rm -rf "$URHO_SRC"
        git clone --depth 1 --branch "$URHO3D_REF" https://github.com/urho3d/Urho3D.git "$URHO_SRC" || \
            git clone --depth 1 https://github.com/urho3d/Urho3D.git "$URHO_SRC"
    fi
    cmake -S "$URHO_SRC" -B "$URHO_BLD" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$LOCAL_SDK" \
        -DURHO3D_LIB_TYPE=STATIC \
        -DURHO3D_C++11=1 \
        -DURHO3D_PCH=0 \
        -DURHO3D_SAMPLES=0 \
        -DURHO3D_TOOLS=0 \
        -DURHO3D_LUA=0 \
        -DURHO3D_LUAJIT=0 \
        -DURHO3D_ANGELSCRIPT=0 \
        -DURHO3D_NETWORK=0 \
        -DURHO3D_DATABASE_ODBC=0 \
        -DURHO3D_DATABASE_SQLITE=0 \
        -DURHO3D_URHO2D=0 \
        -DURHO3D_WEBP=0 \
        -DURHO3D_TESTING=0 \
        -DURHO3D_DOCS=0
    cmake --build "$URHO_BLD" -j"$JOBS"
    cmake --install "$URHO_BLD"
    rm -rf "$URHO_BLD"
    find_urho3d
fi

if [ -z "$URHO_INC" ] || [ -z "$URHO_LIBDIR" ]; then
    echo "ОШИБКА: Не удалось найти или собрать Urho3D SDK."
    exit 1
fi
echo "[Urho3D] Заголовки:  $URHO_INC"
echo "[Urho3D] Библиотеки: $URHO_LIBDIR"

# ---- 3. Флаги SDL2, Bullet3 и системных библиотек ----
SDL_INC=""
SDL_INC2=""
SDL_LIBS="-lSDL2"
if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists sdl2 2>/dev/null; then
    SDL_LIBS="$(pkg-config --libs sdl2)"
    for f in $(pkg-config --cflags-only-I sdl2 | tr ' ' '\n' | sed 's/^-I//'); do
        if [ -f "$f/SDL/SDL_joystick.h" ]; then SDL_INC="$f"; break; fi
        if [ -f "$f/SDL2/SDL_joystick.h" ]; then SDL_INC2="$f/SDL2"; fi
        if [ -f "$f/SDL_joystick.h" ] && [ "$(basename "$f")" = "SDL2" ]; then SDL_INC2="$f"; fi
    done
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

BULLET_LIBS=()
for lib in Bullet3Collision Bullet3Dynamics Bullet3Geometry BulletInverseDynamics; do
    if ls "$URHO_LIBDIR/lib$lib".so* "$URHO_LIBDIR/lib$lib".a >/dev/null 2>&1; then
        BULLET_LIBS+=("-l$lib")
    fi
done

# Если используем статический libUrho3D.a, подключаем системные аудио/графика/udev либы
EXTRA_SYS_LIBS=()
if [ -f "$URHO_LIBDIR/libUrho3D.a" ] && [ ! -f "$URHO_LIBDIR/libUrho3D.so" ]; then
    if [ "$(uname -s)" = "Linux" ]; then
        for syslib in GL GLU X11 Xext Xrandr Xi asound pulse pulse-simple rt dl m; do
            if ldconfig -p 2>/dev/null | grep -q "lib${syslib}\.so" || [ -f "/usr/lib/x86_64-linux-gnu/lib${syslib}.so" ]; then
                EXTRA_SYS_LIBS+=("-l$syslib")
            fi
        done
    elif [ "$(uname -s)" = "Darwin" ]; then
        EXTRA_SYS_LIBS+=("-framework" "OpenGL" "-framework" "Cocoa" "-framework" "IOKit" "-framework" "CoreAudio" "-framework" "AudioUnit" "-framework" "ForceFeedback" "-framework" "Carbon" "-framework" "Metal")
    fi
else
    if [ "$(uname -s)" = "Linux" ]; then
        EXTRA_SYS_LIBS+=("-lGL" "-lX11" "-lrt" "-ldl" "-lm")
    fi
fi

# ---- 4. Синхронизация звуков, шейдеров, материалов и ресурсов ----
echo "[1/3] Синхронизация звуков, GLSL шейдеров, материалов и ресурсов..."
if [ ! -f "$ROOT/Data/Sounds/BanyaSteam.wav" ] || [ ! -f "$ROOT/Data/Sounds/Shot_Railgun.wav" ]; then
    python3 "$ROOT/tools/gen_sounds.py"
fi

mkdir -p "$ROOT/build/CoreData" "$ROOT/build/Data"
URHO_RES_PREFIX="$(dirname "$URHO_INC")"
for csrc in \
    "$LOCAL_SDK/share/Urho3D/Resources/CoreData" \
    "$LOCAL_SDK/share/Urho3D/CoreData" \
    "${URHO3D_HOME:-}/share/Urho3D/Resources/CoreData" \
    "${URHO3D_HOME:-}/bin/CoreData" \
    "$URHO_RES_PREFIX/share/Urho3D/Resources/CoreData" \
    "$URHO_RES_PREFIX/bin/CoreData" \
    "$HOME/urho3d/bin/CoreData" \
    "/usr/local/share/Urho3D/Resources/CoreData" \
    "/usr/local/share/Urho3D/CoreData" \
    "$ROOT/CoreData"
do
    [ -n "$csrc" ] && [ -d "$csrc" ] && cp -rf "$csrc"/. "$ROOT/build/CoreData"/ 2>/dev/null || true
done

# Копируем также CoreData в корень только если её там не хватало от Urho3D
if [ ! -d "$ROOT/CoreData/Shaders" ] && [ -d "$ROOT/build/CoreData/Shaders" ]; then
    cp -rf "$ROOT/build/CoreData"/. "$ROOT/CoreData"/ 2>/dev/null || true
fi

for dsrc in \
    "$LOCAL_SDK/share/Urho3D/Resources/Data" \
    "$LOCAL_SDK/share/Urho3D/Data" \
    "${URHO3D_HOME:-}/share/Urho3D/Resources/Data" \
    "${URHO3D_HOME:-}/bin/Data" \
    "$URHO_RES_PREFIX/share/Urho3D/Resources/Data" \
    "$URHO_RES_PREFIX/bin/Data" \
    "$HOME/urho3d/bin/Data" \
    "/usr/local/share/Urho3D/Resources/Data" \
    "/usr/local/share/Urho3D/Data" \
    "$ROOT/Data"
do
    [ -n "$dsrc" ] && [ -d "$dsrc" ] && cp -rf "$dsrc"/. "$ROOT/build/Data"/ 2>/dev/null || true
done

rm -rf "$ROOT/build/Data/Data" "$ROOT/build/Data/CoreData" 2>/dev/null || true
find "$ROOT/build/Data" -name "*.mat" ! -path "*Materials*" -delete 2>/dev/null || true
find "$ROOT/build/Data" "$ROOT/build/CoreData" -type f -size 0 -delete 2>/dev/null || true
[ -f "$ROOT/bin/settings.ini" ] && [ ! -f "$ROOT/build/settings.ini" ] && cp "$ROOT/bin/settings.ini" "$ROOT/build/settings.ini" || true

# ---- 5. Валидация и генерация 9 PBR наборов текстур (вкл. BanyaLog & BanyaRooster) ----
texture_set_is_valid() {
    local tex_dir="$1"
    python3 - "$tex_dir" <<'PY'
import os, struct, sys, zlib

tex_dir = sys.argv[1]
required = [
    "StoneHDDiffuse.png", "StoneHDNormal.png", "StoneHDRoughMetal.png",
    "FloorTileDiffuse.png", "FloorTileNormal.png", "FloorTileRoughMetal.png",
    "MetalPanelDiffuse.png", "MetalPanelNormal.png", "MetalPanelRoughMetal.png",
    "BrushedMetalDiffuse.png", "BrushedMetalNormal.png", "BrushedMetalRoughMetal.png",
    "CrateWoodDiffuse.png", "CrateWoodNormal.png", "CrateWoodRoughMetal.png",
    "FeatherPBRDiffuse.png", "FeatherPBRNormal.png", "FeatherPBRRoughMetal.png",
    "HazardMetalDiffuse.png", "HazardMetalNormal.png", "HazardMetalRoughMetal.png",
    "BanyaLogDiffuse.png", "BanyaLogNormal.png", "BanyaLogRoughMetal.png",
    "BanyaRoosterDiffuse.png", "BanyaRoosterNormal.png", "BanyaRoosterRoughMetal.png",
    "Sky4K_PosX.png", "Sky4K_NegX.png", "Sky4K_PosY.png",
    "Sky4K_NegY.png", "Sky4K_PosZ.png", "Sky4K_NegZ.png",
    "LoadingBg4K.png",
]
if not os.path.isfile(os.path.join(tex_dir, "Skybox4K.xml")):
    sys.exit(1)

for name in required:
    path = os.path.join(tex_dir, name)
    if not os.path.isfile(path) or os.path.getsize(path) < 64:
        sys.exit(1)
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        sys.exit(1)
    pos, w, h, bit_depth, color_type = 8, None, None, None, None
    idat = bytearray()
    saw_iend = False
    while pos + 12 <= len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        ctype = data[pos + 4:pos + 8]
        chunk_end = pos + 8 + length
        if chunk_end + 4 > len(data):
            sys.exit(1)
        payload = data[pos + 8:chunk_end]
        if ctype == b"IHDR":
            w, h, bit_depth, color_type = struct.unpack(">IIBB", payload[:10])
        elif ctype == b"IDAT":
            idat.extend(payload)
        elif ctype == b"IEND":
            saw_iend = True
            break
        pos = chunk_end + 4
    if not saw_iend or not w or not h or bit_depth != 8 or color_type not in (2, 6):
        sys.exit(1)
    channels = 3 if color_type == 2 else 4
    expected = h * (1 + w * channels)
    try:
        raw = zlib.decompress(bytes(idat))
    except Exception:
        sys.exit(1)
    if len(raw) != expected:
        sys.exit(1)
sys.exit(0)
PY
}

STAMP_FILE="$ROOT/build/Data/Textures/.4k_pbr_stamp"
CUR_STAMP=""
[ -f "$STAMP_FILE" ] && CUR_STAMP="$(cat "$STAMP_FILE" 2>/dev/null || true)"

if [ "$CUR_STAMP" != "$TEX_SIZE" ] || ! texture_set_is_valid "$ROOT/build/Data/Textures"; then
    echo "[2/3] Генерация PBR текстур (${TEX_SIZE}x${TEX_SIZE}), IBL Skybox4K и UHD Loading Art..."
    "$CXX" -O3 -std=c++17 -pthread "$ROOT/tools/gen_textures_4k.cpp" -o "$ROOT/build/gen_textures_4k"
    "$ROOT/build/gen_textures_4k" "$ROOT/build/Data/Textures" "$TEX_SIZE"
    if ! texture_set_is_valid "$ROOT/build/Data/Textures"; then
        echo "ОШИБКА: Не удалось подтвердить целостность PNG-набора в build/Data/Textures!"
        exit 1
    fi
else
    echo "[2/3] PBR текстуры (${TEX_SIZE}x${TEX_SIZE}) уже актуальны и валидны."
fi

# ---- 6. Компиляция нативного клиента ----
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
    -Wl,-rpath,'$ORIGIN/lib' \
    -lUrho3D \
    "${BULLET_LIBS[@]}" \
    $SDL_LIBS \
    "${EXTRA_SYS_LIBS[@]}" \
    -lpthread

echo "================================================================"
echo "Готово! Нативный клиент собран: build/MyShooter"
echo "  Запуск одной командой:   ./run.sh"
echo "  В 4K качестве:           ./build.sh --4k && ./run.sh"
echo "  Портативный релиз Linux: ./tools/package_linux.sh"
echo "================================================================"
