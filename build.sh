#!/usr/bin/env bash
# ============================================================================
# CHIKENGUN 2.5 — ПЕРО СУДЬБЫ (Zero-Pain Cross-Platform Native Builder)
# Поддержка: Nix / NixOS, Ubuntu/Debian, Fedora/RHEL, Arch, openSUSE,
#            Alpine Linux (musl), Void Linux, macOS (Intel & Apple Silicon),
#            Windows (MSYS2 / MinGW64; для MSVC см. build.ps1 / run.bat).
#
# Флаги:
#   --install-deps   Автоматически поставить пакеты сборки / войти в nix-shell
#   --4k             Сгенерировать PBR текстуры в максимальном 4096x4096 (def: 1024)
#   --tests-only     Собрать и прогнать только модульные C++17 тесты (без Urho3D)
#   -j N / --jobs N  Число потоков сборки (по умолчанию все ядра)
# ============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
ORIG_ARGS=("$@")
CXX="${CXX:-}"
CXXFLAGS="${CXXFLAGS:--O2 -DNDEBUG -Wall -Wextra}"
TEX_SIZE="${TEX_SIZE:-1024}"
URHO3D_REF="${URHO3D_REF:-1.9.0}"
INSTALL_DEPS="${AUTO_INSTALL_DEPS:-0}"
TESTS_ONLY=0
JOBS=""

OS_NAME="$(uname -s)"
IS_WIN=0
EXE_EXT=""
case "$OS_NAME" in
    MINGW*|MSYS*|CYGWIN*)
        IS_WIN=1
        EXE_EXT=".exe"
        ;;
esac

IS_NIX=0
if [ -f /etc/NIXOS ] || [ -n "${IN_NIX_SHELL:-}" ] || [ -n "${NIX_CC:-}" ] || [ -n "${NIX_CFLAGS_COMPILE:-}" ]; then
    IS_NIX=1
fi

OUT="$ROOT/build/MyShooter${EXE_EXT}"

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
        JOBS="${NUMBER_OF_PROCESSORS:-4}"
    fi
fi

pick_cxx() {
    if [ -n "${CXX:-}" ] && command -v "$CXX" >/dev/null 2>&1; then
        return 0
    fi
    for cand in g++ clang++ c++; do
        if command -v "$cand" >/dev/null 2>&1; then
            CXX="$cand"
            return 0
        fi
    done
    return 1
}

mkdir -p "$ROOT/build"

# ---- Быстрая ветка: только headless C++17 тесты ----
if [ "$TESTS_ONLY" -eq 1 ]; then
    pick_cxx || {
        echo "ОШИБКА: C++ компилятор (g++/clang++) не найден."
        exit 1
    }
    echo "[Tests] Компиляция gen_textures_4k + test_core ($CXX)..."
    "$CXX" -O3 -std=c++17 -pthread "$ROOT/tools/gen_textures_4k.cpp" -o "$ROOT/build/gen_textures_4k${EXE_EXT}"
    "$CXX" -O2 -std=c++17 -Wall -Wextra -Werror -UNDEBUG "$ROOT/tests/test_core.cpp" -o "$ROOT/build/test_core${EXE_EXT}"
    "$ROOT/build/test_core${EXE_EXT}" "$ROOT/build/gen_textures_4k${EXE_EXT}"
    exit 0
fi

# ---- Вспомогательные проверки через реальный препроцессор/линковщик (NixOS/Alpine/macOS safe) ----
has_cpp_header() {
    local hdr="$1"
    shift || true
    [ -n "${CXX:-}" ] || return 1
    printf '#include <%s>\nint main(){return 0;}\n' "$hdr" | "$CXX" -x c++ -E "$@" - >/dev/null 2>&1
}

can_link_lib() {
    local lib="$1"
    [ -n "${CXX:-}" ] || return 1
    printf 'int main(){return 0;}\n' | "$CXX" -x c++ - -o "$ROOT/build/.link_probe${EXE_EXT}" "-l$lib" >/dev/null 2>&1
    local rc=$?
    rm -f "$ROOT/build/.link_probe${EXE_EXT}"
    return $rc
}

can_link_framework() {
    local fw="$1"
    [ -n "${CXX:-}" ] || return 1
    printf 'int main(){return 0;}\n' | "$CXX" -x c++ - -o "$ROOT/build/.link_probe" -framework "$fw" >/dev/null 2>&1
    local rc=$?
    rm -f "$ROOT/build/.link_probe"
    return $rc
}

# ---- 1. Проверка и автоустановка системных зависимостей / Nix devShell ----
detect_pkg_mgr() {
    if [ "$IS_NIX" -eq 1 ] && [ -z "${IN_NIX_SHELL:-}" ] && command -v nix >/dev/null 2>&1; then
        echo "nix"
        return
    fi
    if command -v apt-get >/dev/null 2>&1; then echo "apt"; return; fi
    if command -v dnf >/dev/null 2>&1; then echo "dnf"; return; fi
    if command -v pacman >/dev/null 2>&1; then echo "pacman"; return; fi
    if command -v zypper >/dev/null 2>&1; then echo "zypper"; return; fi
    if command -v apk >/dev/null 2>&1; then echo "apk"; return; fi
    if command -v xbps-install >/dev/null 2>&1; then echo "xbps"; return; fi
    if command -v brew >/dev/null 2>&1; then echo "brew"; return; fi
    echo "unknown"
}

check_missing_tools() {
    local missing=()
    pick_cxx || missing+=("c++-compiler")
    command -v cmake >/dev/null 2>&1 || missing+=("cmake")
    if ! command -v make >/dev/null 2>&1 && ! command -v ninja >/dev/null 2>&1; then
        missing+=("make")
    fi
    command -v git >/dev/null 2>&1 || missing+=("git")
    command -v python3 >/dev/null 2>&1 || missing+=("python3")

    if [ "$OS_NAME" = "Linux" ] && [ -n "${CXX:-}" ]; then
        local has_sdl=0 has_gl=0 has_x11=0
        if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists sdl2 2>/dev/null; then
            has_sdl=1
        elif has_cpp_header "SDL2/SDL.h" || has_cpp_header "SDL.h"; then
            has_sdl=1
        fi
        if (command -v pkg-config >/dev/null 2>&1 && pkg-config --exists gl 2>/dev/null) || has_cpp_header "GL/gl.h"; then
            has_gl=1
        fi
        if (command -v pkg-config >/dev/null 2>&1 && pkg-config --exists x11 2>/dev/null) || has_cpp_header "X11/Xlib.h"; then
            has_x11=1
        fi
        [ "$has_sdl" -eq 0 ] && missing+=("sdl2-dev")
        [ "$has_gl" -eq 0 ] && missing+=("opengl-dev")
        [ "$has_x11" -eq 0 ] && missing+=("x11-dev")
    elif [ "$OS_NAME" = "Darwin" ] && [ -n "${CXX:-}" ]; then
        if ! (command -v pkg-config >/dev/null 2>&1 && pkg-config --exists sdl2 2>/dev/null) && \
           ! has_cpp_header "SDL2/SDL.h" && \
           [ ! -d "/opt/homebrew/include/SDL2" ] && [ ! -d "/usr/local/include/SDL2" ]; then
            missing+=("sdl2")
        fi
    fi
    echo "${missing[*]:-}"
}

install_cmd_for_mgr() {
    case "$1" in
        apt)
            echo "sudo apt-get update && sudo apt-get install -y build-essential cmake git pkg-config python3 libsdl2-dev libgl-dev libglu1-mesa-dev libx11-dev libxrandr-dev libxi-dev libxext-dev libxcb1-dev libasound2-dev libpulse-dev libudev-dev"
            ;;
        dnf)
            echo "sudo dnf install -y gcc-c++ cmake make git pkgconf-pkg-config python3 SDL2-devel mesa-libGL-devel mesa-libGLU-devel libX11-devel libXrandr-devel libXi-devel libXext-devel libxcb-devel alsa-lib-devel pulseaudio-libs-devel systemd-devel"
            ;;
        pacman)
            if [ "$IS_WIN" -eq 1 ]; then
                echo "pacman -S --needed --noconfirm mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-SDL2 mingw-w64-x86_64-pkgconf git python3"
            else
                echo "sudo pacman -S --needed --noconfirm base-devel cmake git pkgconf python sdl2 mesa glu libx11 libxrandr libxi libxext libxcb alsa-lib libpulse"
            fi
            ;;
        zypper)
            echo "sudo zypper install -y gcc-c++ cmake make git pkg-config python3 libSDL2-devel Mesa-libGL-devel Mesa-libGLU-devel libX11-devel libXrandr-devel libXi-devel libXext-devel libxcb-devel alsa-devel libpulse-devel systemd-devel"
            ;;
        apk)
            echo "sudo apk add --no-cache build-base cmake git pkgconf python3 sdl2-dev mesa-dev glu-dev libx11-dev libxrandr-dev libxi-dev libxext-dev libxcb-dev alsa-lib-dev pulseaudio-dev eudev-dev linux-headers libexecinfo-dev"
            ;;
        xbps)
            echo "sudo xbps-install -Sy base-devel cmake git pkg-config python3 SDL2-devel MesaLib-devel glu-devel libX11-devel libXrandr-devel libXi-devel libXext-devel libxcb-devel alsa-lib-devel pulseaudio-devel eudev-libudev-devel"
            ;;
        brew)
            echo "brew install cmake sdl2 python pkg-config"
            ;;
        *)
            echo ""
            ;;
    esac
}

MISSING="$(check_missing_tools)"
if [ -n "$MISSING" ]; then
    # Особая ветка для NixOS / Nix вне devShell / nix-shell
    if [ "$IS_NIX" -eq 1 ] && [ -z "${IN_NIX_SHELL:-}" ]; then
        echo "[Nix] Обнаружена система Nix/NixOS (недостающие инструменты в текущем shell: $MISSING)."
        if [ "$INSTALL_DEPS" -eq 1 ] || [ -t 0 ]; then
            if command -v nix >/dev/null 2>&1 && nix --extra-experimental-features "nix-command flakes" flake metadata "$ROOT" >/dev/null 2>&1; then
                echo "[Nix] Перезапускаю сборку внутри nix develop..."
                exec nix --extra-experimental-features "nix-command flakes" develop "$ROOT" -c "$0" "${ORIG_ARGS[@]}"
            elif command -v nix-shell >/dev/null 2>&1 && [ -f "$ROOT/shell.nix" ]; then
                echo "[Nix] Перезапускаю сборку внутри nix-shell..."
                exec nix-shell "$ROOT/shell.nix" --run "$(printf '%q ' "$0" "${ORIG_ARGS[@]}")"
            fi
        fi
        echo "Запустите одним из способов:"
        echo "  nix run                                  # Собрать и сразу запустить (Flakes)"
        echo "  nix develop -c ./run.sh                  # Войти в devShell и запустить"
        echo "  nix-shell --run ./run.sh                 # Классический nix-shell (без Flakes)"
        exit 1
    fi

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
            echo "  Потребуются: C++17 компилятор, CMake, make/ninja, git, python3, SDL2, OpenGL (и X11/ALSA/PulseAudio на Linux)"
        fi
        echo "Или запустите: ./build.sh --install-deps"
        exit 1
    fi
fi

pick_cxx || {
    echo "ОШИБКА: C++ компилятор так и не найден."
    exit 1
}

# ---- 2. Автопоиск заголовков и библиотеки Urho3D (включая Nix store, pkg-config, .cache/urho3d-sdk) ----
LOCAL_SDK="$ROOT/.cache/urho3d-sdk"
URHO_PC_DIR=""
find_urho3d() {
    URHO_INC=""
    URHO_LIBDIR=""
    URHO_PC_DIR=""

    # Проверяем Urho3D.pc через pkg-config
    local pc_paths=""
    for pd in \
        "${URHO3D_HOME:-}/lib/pkgconfig" \
        "${URHO3D_HOME:-}/lib64/pkgconfig" \
        "$LOCAL_SDK/lib/pkgconfig" \
        "$LOCAL_SDK/lib64/pkgconfig"; do
        [ -d "$pd" ] && pc_paths="${pc_paths:+$pc_paths:}$pd"
    done
    [ -n "$pc_paths" ] && export PKG_CONFIG_PATH="${pc_paths}${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

    local candidates=()
    [ -n "${URHO_INC_OVERRIDE:-}" ] && candidates+=("$URHO_INC_OVERRIDE")
    [ -n "${URHO3D_HOME:-}" ]       && candidates+=("$URHO3D_HOME/include" "$URHO3D_HOME/build/include")
    candidates+=(
        "$LOCAL_SDK/include"
        "$HOME/urho3d/build/include"
        "$HOME/urho3d_build/include"
        "$HOME/urho3d-local/include"
        "/opt/homebrew/include/Urho3D"
        "/opt/homebrew/include"
        "/usr/local/include/Urho3D"
        "/usr/local/include"
        "/usr/include/Urho3D"
        "/usr/include"
    )
    if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists Urho3D 2>/dev/null; then
        local pc_inc pc_lib
        pc_inc="$(pkg-config --variable=includedir Urho3D 2>/dev/null || true)"
        pc_lib="$(pkg-config --variable=libdir Urho3D 2>/dev/null || true)"
        [ -n "$pc_inc" ] && candidates=("$pc_inc" "${candidates[@]}")
        [ -n "$pc_lib" ] && [ -d "$pc_lib" ] && URHO_PC_DIR="$pc_lib"
    fi

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
    [ -n "$URHO_PC_DIR" ]    && libdirs+=("$URHO_PC_DIR" "$URHO_PC_DIR/Urho3D")
    [ -n "${URHO3D_HOME:-}" ] && libdirs+=("$URHO3D_HOME/lib" "$URHO3D_HOME/lib/Urho3D" "$URHO3D_HOME/lib64" "$URHO3D_HOME/lib64/Urho3D" "$URHO3D_HOME/build/lib" "$URHO3D_HOME/build/lib/Urho3D")
    [ -n "$URHO_INC" ]       && libdirs+=("$(dirname "$URHO_INC")/lib" "$(dirname "$URHO_INC")/lib/Urho3D" "$(dirname "$URHO_INC")/lib64" "$(dirname "$URHO_INC")/lib64/Urho3D")
    libdirs+=(
        "$LOCAL_SDK/lib"
        "$LOCAL_SDK/lib/Urho3D"
        "$LOCAL_SDK/lib64"
        "$LOCAL_SDK/lib64/Urho3D"
        "$HOME/urho3d/build/lib"
        "$HOME/urho3d_build/lib"
        "/opt/homebrew/lib"
        "/opt/homebrew/lib/Urho3D"
        "/usr/local/lib"
        "/usr/local/lib/Urho3D"
        "/usr/local/lib64"
        "/usr/local/lib64/Urho3D"
        "/usr/lib/x86_64-linux-gnu"
        "/usr/lib/aarch64-linux-gnu"
        "/usr/lib64"
        "/usr/lib64/Urho3D"
        "/usr/lib"
        "/usr/lib/Urho3D"
    )
    for ld in "${libdirs[@]}"; do
        [ -z "$ld" ] && continue
        if [ -f "$ld/libUrho3D.a" ] || [ -f "$ld/libUrho3D.so" ] || [ -f "$ld/libUrho3D.dylib" ] || [ -f "$ld/libUrho3D.dll.a" ]; then
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

    CMAKE_EXTRA_ARGS=(
        "-DLIB_SUFFIX="
        "-DURHO3D_USE_LIB_DEB=1"
        "-DCMAKE_C_FLAGS=-std=gnu11 -Wno-error -Wno-implicit-function-declaration -Wno-incompatible-pointer-types"
        "-DCMAKE_CXX_FLAGS=-include cstdint -Wno-error"
    )
    # CMake >= 3.27 / 4.0 отклоняет старые ThirdParty cmake_minimum_required без policy minimum
    CMAKE_VER="$(cmake --version 2>/dev/null | head -n1 | sed -E 's/[^0-9]*([0-9]+\.[0-9]+).*/\1/' || echo "3.20")"
    CMAKE_MAJ="${CMAKE_VER%%.*}"
    CMAKE_MIN="${CMAKE_VER#*.}"; CMAKE_MIN="${CMAKE_MIN%%.*}"
    if [ "${CMAKE_MAJ:-0}" -gt 3 ] || { [ "${CMAKE_MAJ:-0}" -eq 3 ] && [ "${CMAKE_MIN:-0}" -ge 27 ]; }; then
        CMAKE_EXTRA_ARGS+=("-DCMAKE_POLICY_VERSION_MINIMUM=3.5")
    fi
    if [ "$IS_WIN" -eq 1 ]; then
        CMAKE_EXTRA_ARGS+=("-DURHO3D_OPENGL=1" "-DURHO3D_WIN32_CONSOLE=0")
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
        -DURHO3D_DOCS=0 \
        "${CMAKE_EXTRA_ARGS[@]}"
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

# ---- 3. Флаги SDL2, Bullet3, статического Urho3D и системных библиотек ----
SDL_INC=""
SDL_INC2=""
SDL_LIBS=""

if [ -d "$URHO_INC/Urho3D/ThirdParty/SDL" ] && [ -f "$URHO_INC/Urho3D/ThirdParty/SDL/SDL_joystick.h" ]; then
    SDL_INC="$URHO_INC/Urho3D/ThirdParty"
fi

if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists sdl2 2>/dev/null; then
    # Если Urho3D собран с системным SDL2 или динамически — добавим -lSDL2
    if [ -z "$SDL_INC" ] || [ ! -f "$URHO_LIBDIR/libUrho3D.a" ]; then
        SDL_LIBS="$(pkg-config --libs sdl2)"
    fi
    for f in $(pkg-config --cflags-only-I sdl2 | tr ' ' '\n' | sed 's/^-I//'); do
        if [ -z "$SDL_INC" ] && [ -f "$f/SDL/SDL_joystick.h" ]; then SDL_INC="$f"; break; fi
        if [ -f "$f/SDL2/SDL_joystick.h" ]; then SDL_INC2="$f/SDL2"; fi
        if [ -f "$f/SDL_joystick.h" ] && [ "$(basename "$f")" = "SDL2" ]; then SDL_INC2="$f"; fi
    done
fi

if [ -z "$SDL_INC" ]; then
    for d in \
        "$URHO_INC/Urho3D/ThirdParty" \
        "/opt/homebrew/include" \
        "/usr/local/include" \
        "/usr/include"
    do
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
    if ls "$URHO_LIBDIR/lib$lib".so* "$URHO_LIBDIR/lib$lib".a "$URHO_LIBDIR/lib$lib".dylib >/dev/null 2>&1; then
        BULLET_LIBS+=("-l$lib")
    fi
done

EXTRA_DEFINES=()
EXTRA_SYS_LIBS=()
RPATH_FLAGS=()

if [ -f "$URHO_LIBDIR/libUrho3D.a" ] && [ ! -f "$URHO_LIBDIR/libUrho3D.so" ] && [ ! -f "$URHO_LIBDIR/libUrho3D.dylib" ] && [ ! -f "$URHO_LIBDIR/libUrho3D.dll.a" ]; then
    EXTRA_DEFINES+=("-DURHO3D_STATIC_DEFINE")
fi

# Если есть Urho3D.pc (генерируется CMake'ом Urho3D), берём системные флаги прямо оттуда + перепроверяем
URHO_PC_LIBS=""
if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists Urho3D 2>/dev/null; then
    URHO_PC_LIBS="$(pkg-config --libs Urho3D 2>/dev/null || true)"
fi

if [ "$OS_NAME" = "Linux" ]; then
    RPATH_FLAGS+=("-Wl,-rpath,$URHO_LIBDIR" "-Wl,-rpath,\$ORIGIN/lib")
    if [ -n "${EXTRA_DEFINES[*]:-}" ]; then
        # Сначала пробуем pkg-config для NixOS / нестандартных путей, затем probe через линковщик
        if command -v pkg-config >/dev/null 2>&1; then
            for pc_mod in gl glu x11 xext xrandr xi alsa libpulse libpulse-simple libudev; do
                if pkg-config --exists "$pc_mod" 2>/dev/null; then
                    for tok in $(pkg-config --libs "$pc_mod" 2>/dev/null); do
                        EXTRA_SYS_LIBS+=("$tok")
                    done
                fi
            done
        fi
        for syslib in GL GLU X11 Xext Xrandr Xi asound pulse pulse-simple udev rt dl m; do
            if can_link_lib "$syslib"; then
                case " ${EXTRA_SYS_LIBS[*]:-} " in
                    *" -l$syslib "*) ;;
                    *) EXTRA_SYS_LIBS+=("-l$syslib") ;;
                esac
            fi
        done
    else
        EXTRA_SYS_LIBS+=("-lGL" "-lX11" "-lrt" "-ldl" "-lm")
        [ -z "$SDL_LIBS" ] && can_link_lib SDL2 && SDL_LIBS="-lSDL2"
    fi
elif [ "$OS_NAME" = "Darwin" ]; then
    RPATH_FLAGS+=("-Wl,-rpath,$URHO_LIBDIR" "-Wl,-rpath,@executable_path/lib")
    for fw in OpenGL Cocoa IOKit CoreAudio AudioUnit AudioToolbox CoreVideo QuartzCore CoreFoundation ForceFeedback Carbon Metal GameController; do
        if can_link_framework "$fw"; then
            EXTRA_SYS_LIBS+=("-framework" "$fw")
        fi
    done
    can_link_lib iconv && EXTRA_SYS_LIBS+=("-liconv")
elif [ "$IS_WIN" -eq 1 ]; then
    EXTRA_DEFINES+=("-DNOMINMAX" "-DWIN32_LEAN_AND_MEAN" "-Wa,-mbig-obj")
    EXTRA_SYS_LIBS+=(
        "-mwindows"
        "-lopengl32" "-lglu32" "-lgdi32" "-luser32" "-lwinmm"
        "-limm32" "-lole32" "-loleaut32" "-lversion" "-luuid"
        "-lsetupapi" "-lshell32" "-ladvapi32" "-lws2_32" "-liphlpapi" "-ldbghelp"
    )
fi

# ---- 4. Синхронизация звуков, шейдеров, материалов и ресурсов ----
echo "[1/3] Синхронизация звуков, GLSL шейдеров, материалов и ресурсов..."
if [ ! -f "$ROOT/Data/Sounds/BanyaSteam.wav" ] || [ ! -f "$ROOT/Data/Sounds/Shot_Railgun.wav" ]; then
    python3 "$ROOT/tools/gen_sounds.py"
fi

mkdir -p "$ROOT/build/CoreData" "$ROOT/build/Data"
URHO_RES_PREFIX="$(dirname "$URHO_INC")"
[ "$(basename "$URHO_RES_PREFIX")" = "include" ] && URHO_RES_PREFIX="$(dirname "$URHO_RES_PREFIX")"
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
    if [ -n "$csrc" ] && [ -d "$csrc" ]; then
        cp -rf "$csrc"/. "$ROOT/build/CoreData"/ 2>/dev/null || true
        chmod -R u+w "$ROOT/build/CoreData" 2>/dev/null || true
    fi
done

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
    if [ -n "$dsrc" ] && [ -d "$dsrc" ]; then
        cp -rf "$dsrc"/. "$ROOT/build/Data"/ 2>/dev/null || true
        chmod -R u+w "$ROOT/build/Data" 2>/dev/null || true
    fi
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
    "$CXX" -O3 -std=c++17 -pthread "$ROOT/tools/gen_textures_4k.cpp" -o "$ROOT/build/gen_textures_4k${EXE_EXT}"
    "$ROOT/build/gen_textures_4k${EXE_EXT}" "$ROOT/build/Data/Textures" "$TEX_SIZE"
    if ! texture_set_is_valid "$ROOT/build/Data/Textures"; then
        echo "ОШИБКА: Не удалось подтвердить целостность PNG-набора в build/Data/Textures!"
        exit 1
    fi
else
    echo "[2/3] PBR текстуры (${TEX_SIZE}x${TEX_SIZE}) уже актуальны и валидны."
fi

# ---- 6. Компиляция нативного клиента ----
echo "[3/3] Компиляция src/MyApp.cpp -> build/MyShooter${EXE_EXT} ($CXX)..."
"$CXX" "$ROOT/src/MyApp.cpp" \
    -o "$OUT" \
    -std=c++17 $CXXFLAGS \
    "${EXTRA_DEFINES[@]}" \
    -isystem "$URHO_INC" \
    -isystem "$URHO_INC/Urho3D/ThirdParty" \
    $SDL_CFLAGS \
    $BULLET_CFLAGS \
    -L"$URHO_LIBDIR" \
    "${RPATH_FLAGS[@]}" \
    -lUrho3D \
    "${BULLET_LIBS[@]}" \
    $SDL_LIBS \
    "${EXTRA_SYS_LIBS[@]}" \
    -lpthread

echo "================================================================"
echo "Готово! Нативный клиент собран: build/MyShooter${EXE_EXT}"
echo "  Запуск одной командой:   ./run.sh"
echo "  В 4K качестве:           ./build.sh --4k && ./run.sh"
if [ "$OS_NAME" = "Linux" ]; then
    echo "  Портативный релиз Linux: ./tools/package_linux.sh"
fi
echo "================================================================"
