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
if [ -z "${BASH_VERSION:-}" ]; then exec bash "$0" "$@"; fi
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
if [ -f /etc/NIXOS ] || [ -n "${IN_NIX_SHELL:-}" ] || [ -n "${NIX_CC:-}" ] || [ -n "${NIX_CFLAGS_COMPILE:-}" ] || [ -n "${NIX_PROFILES:-}" ] || [ -d "$HOME/.nix-profile" ]; then
    IS_NIX=1
fi

NIX_INC_FLAGS=()
NIX_LINK_FLAGS=()
NIX_RPATH_DIRS=()

append_unique_word() {
    local arr_name="$1" val="$2"
    [ -n "$val" ] || return 0
    local existing=""
    eval "existing=\" \${${arr_name}[*]+\${${arr_name}[*]}} \""
    case "$existing" in
        *" $val "*) return 0 ;;
    esac
    eval "${arr_name}+=(\"\$val\")"
}

append_unique_inc_dir() {
    local dir="$1"
    [ -n "$dir" ] && [ -d "$dir" ] || return 0
    case " ${NIX_INC_FLAGS[*]+${NIX_INC_FLAGS[*]}} " in
        *" $dir "*) return 0 ;;
    esac
    NIX_INC_FLAGS+=("-isystem" "$dir")
}

collect_nix_paths() {
    [ "$IS_NIX" -eq 1 ] || return 0
    local profiles=()
    local p
    if [ -n "${NIX_PROFILES:-}" ]; then
        for p in $NIX_PROFILES; do
            profiles+=("$p")
        done
    fi
    profiles+=(
        "$HOME/.nix-profile"
        "/etc/profiles/per-user/${USER:-$(id -un 2>/dev/null || true)}"
        "/nix/var/nix/profiles/default"
        "/run/current-system/sw"
    )

    local pc_extra=""
    for p in "${profiles[@]}"; do
        [ -n "$p" ] && [ -d "$p" ] || continue
        if [ -d "$p/lib/pkgconfig" ]; then
            pc_extra="${pc_extra:+$pc_extra:}$p/lib/pkgconfig"
        fi
        if [ -d "$p/share/pkgconfig" ]; then
            pc_extra="${pc_extra:+$pc_extra:}$p/share/pkgconfig"
        fi
        if [ -d "$p/include" ]; then
            append_unique_inc_dir "$p/include"
            append_unique_inc_dir "$p/include/SDL2"
        fi
        if [ -d "$p/lib" ]; then
            append_unique_word NIX_LINK_FLAGS "-L$p/lib"
            append_unique_word NIX_RPATH_DIRS "$p/lib"
        fi
        if [ -d "$p/lib64" ]; then
            append_unique_word NIX_LINK_FLAGS "-L$p/lib64"
            append_unique_word NIX_RPATH_DIRS "$p/lib64"
        fi
    done

    if [ -n "${NIX_CFLAGS_COMPILE:-}" ]; then
        local prev="" tok
        for tok in $NIX_CFLAGS_COMPILE; do
            if [ "$prev" = "-isystem" ] || [ "$prev" = "-I" ]; then
                append_unique_inc_dir "$tok"
                prev=""
                continue
            fi
            prev="$tok"
            case "$tok" in
                -I/*)
                    append_unique_inc_dir "${tok#-I}"
                    ;;
            esac
        done
    fi

    if [ -n "${NIX_LDFLAGS:-}" ]; then
        local prev="" tok
        for tok in $NIX_LDFLAGS; do
            if [ "$prev" = "-L" ] && [ -d "$tok" ]; then
                append_unique_word NIX_LINK_FLAGS "-L$tok"
                append_unique_word NIX_RPATH_DIRS "$tok"
                prev=""
                continue
            fi
            prev="$tok"
            case "$tok" in
                -L/*)
                    local d="${tok#-L}"
                    if [ -d "$d" ]; then
                        append_unique_word NIX_LINK_FLAGS "-L$d"
                        append_unique_word NIX_RPATH_DIRS "$d"
                    fi
                    ;;
            esac
        done
    fi

    for gldir in /run/opengl-driver/lib /run/opengl-driver-32/lib; do
        [ -d "$gldir" ] && append_unique_word NIX_RPATH_DIRS "$gldir"
    done

    if [ -n "$pc_extra" ]; then
        export PKG_CONFIG_PATH="${PKG_CONFIG_PATH:+$PKG_CONFIG_PATH:}$pc_extra"
    fi
}

collect_nix_paths

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
    printf '#include <%s>\nint main(){return 0;}\n' "$hdr" | "$CXX" -x c++ -E ${NIX_INC_FLAGS[@]+"${NIX_INC_FLAGS[@]}"} "$@" - >/dev/null 2>&1
}

can_link_lib() {
    local lib="$1"
    [ -n "${CXX:-}" ] || return 1
    printf 'int main(){return 0;}\n' | "$CXX" -x c++ - -o "$ROOT/build/.link_probe${EXE_EXT}" ${NIX_LINK_FLAGS[@]+"${NIX_LINK_FLAGS[@]}"} "-l$lib" >/dev/null 2>&1
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
        elif [ -f "$ROOT/.cache/urho3d-sdk/include/Urho3D/ThirdParty/SDL/SDL_joystick.h" ] || \
             [ -f "$HOME/urho3d-local/include/Urho3D/ThirdParty/SDL/SDL_joystick.h" ] || \
             [ -f "$HOME/urho3d-build/out/include/Urho3D/ThirdParty/SDL/SDL_joystick.h" ]; then
            has_sdl=1
        fi
        if { (command -v pkg-config >/dev/null 2>&1 && pkg-config --exists gl 2>/dev/null) || has_cpp_header "GL/gl.h"; } && \
           { (command -v pkg-config >/dev/null 2>&1 && pkg-config --libs gl >/dev/null 2>&1) || can_link_lib GL || can_link_lib OpenGL; }; then
            has_gl=1
        fi
        if { (command -v pkg-config >/dev/null 2>&1 && pkg-config --exists x11 2>/dev/null) || has_cpp_header "X11/Xlib.h"; } && \
           { (command -v pkg-config >/dev/null 2>&1 && pkg-config --libs x11 >/dev/null 2>&1) || can_link_lib X11; }; then
            has_x11=1
        fi
        [ "$has_sdl" -eq 0 ] && missing+=("sdl2-dev")
        [ "$has_gl" -eq 0 ] && missing+=("opengl-dev")
        [ "$has_x11" -eq 0 ] && missing+=("x11-dev")
    fi
    echo "${missing[*]+${missing[*]}}"
}

install_cmd_for_mgr() {
    case "$1" in
        apt)
            echo "sudo apt-get update && sudo apt-get install -y build-essential cmake git pkg-config python3 libsdl2-dev libgl-dev libglu1-mesa-dev libx11-dev libxrandr-dev libxi-dev libxext-dev libxcursor-dev libxinerama-dev libxss-dev libxcb1-dev libasound2-dev libpulse-dev libdbus-1-dev libudev-dev"
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
        echo "[Nix] Обнаружена система Nix/NixOS (недостающие компоненты в текущем shell: $MISSING)."
        if command -v nix-shell >/dev/null 2>&1 && [ -f "$ROOT/shell.nix" ] && command -v nix-instantiate >/dev/null 2>&1 && nix-instantiate --eval -E '<nixpkgs>' >/dev/null 2>&1; then
            echo "[Nix] Перезапускаю сборку внутри nix-shell..."
            exec nix-shell "$ROOT/shell.nix" --run "$(printf '%q ' "$0" ${ORIG_ARGS[@]+"${ORIG_ARGS[@]}"})"
        elif command -v nix >/dev/null 2>&1 && nix --extra-experimental-features "nix-command flakes" flake metadata "$ROOT" >/dev/null 2>&1; then
            echo "[Nix] Перезапускаю сборку внутри nix develop..."
            exec nix --extra-experimental-features "nix-command flakes" develop "$ROOT" -c "$0" ${ORIG_ARGS[@]+"${ORIG_ARGS[@]}"}
        fi
        echo "Запустите одним из способов:"
        echo "  nix --extra-experimental-features \"nix-command flakes\" run .#default"
        echo "  nix-shell --run ./run.sh                 # Классический nix-shell (без Flakes)"
        echo "  nix develop -c ./run.sh                  # Войти в Flake devShell и запустить"
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

urho_lib_is_usable() {
    local libdir="$1"
    local a="$libdir/libUrho3D.a"
    if [ "$OS_NAME" = "Linux" ] && [ -f "$a" ] && [ ! -f "$libdir/libUrho3D.so" ] && [ "${URHO3D_ALLOW_HEADLESS:-0}" != "1" ] && command -v nm >/dev/null 2>&1; then
        # Если в libUrho3D.a вшит SDL (есть SDL_Init), в нём обязан присутствовать хотя бы один видеодрайвер (X11/Wayland/KMSDRM).
        if nm -g --defined-only "$a" 2>/dev/null | grep -qE ' T _?SDL_Init$'; then
            if ! nm -g --defined-only "$a" 2>/dev/null | grep -qE ' [A-Za-z] _?(X11_bootstrap|Wayland_bootstrap|KMSDRM_bootstrap)$'; then
                return 1
            fi
        fi
    fi
    return 0
}

find_urho_inc_in() {
    local c="$1"
    [ -n "$c" ] || return 1
    if [ -f "$c/Urho3D/Engine/Application.h" ] || [ -f "$c/Urho3D/Urho3D.h" ]; then
        echo "$c"
        return 0
    fi
    if [ -f "$c/Engine/Application.h" ] || [ -f "$c/Core/UrhoVersion.h" ]; then
        dirname "$c"
        return 0
    fi
    return 1
}

find_urho_lib_in() {
    local ld="$1"
    [ -n "$ld" ] || return 1
    if [ -f "$ld/libUrho3D.a" ] || [ -f "$ld/libUrho3D.so" ] || [ -f "$ld/libUrho3D.dylib" ] || [ -f "$ld/libUrho3D.dll.a" ]; then
        echo "$ld"
        return 0
    fi
    return 1
}

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
        "$LOCAL_SDK/lib64/pkgconfig" \
        "$HOME/urho3d-build/out/lib/pkgconfig" \
        "$HOME/urho3d-build/lib/pkgconfig" \
        "$HOME/urho3d_build/out/lib/pkgconfig" \
        "$HOME/urho3d-local/lib/pkgconfig" \
        "$HOME/urho3d-local/lib64/pkgconfig"; do
        [ -d "$pd" ] && pc_paths="${pc_paths:+$pc_paths:}$pd"
    done
    [ -n "$pc_paths" ] && export PKG_CONFIG_PATH="${pc_paths}${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

    local prefixes=()
    [ -n "${URHO3D_HOME:-}" ] && prefixes+=("$URHO3D_HOME" "$URHO3D_HOME/build")
    if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists Urho3D 2>/dev/null; then
        local pc_inc pc_lib
        pc_inc="$(pkg-config --variable=includedir Urho3D 2>/dev/null || true)"
        pc_lib="$(pkg-config --variable=libdir Urho3D 2>/dev/null || true)"
        [ -n "$pc_lib" ] && [ -d "$pc_lib" ] && URHO_PC_DIR="$pc_lib"
        if [ -n "$pc_inc" ] && [ -n "$pc_lib" ]; then
            local cand_inc="" cand_lib=""
            cand_inc="$(find_urho_inc_in "$pc_inc" || true)"
            cand_lib="$(find_urho_lib_in "$pc_lib" || find_urho_lib_in "$pc_lib/Urho3D" || true)"
            if [ -n "$cand_inc" ] && [ -n "$cand_lib" ]; then
                if urho_lib_is_usable "$cand_lib"; then
                    URHO_INC="$cand_inc"
                    URHO_LIBDIR="$cand_lib"
                    return 0
                else
                    echo "[Urho3D] ВНИМАНИЕ: SDK из pkg-config ($cand_lib) собран без видеодрайверов SDL (X11/Wayland), пропускаю." >&2
                fi
            fi
        fi
    fi

    prefixes+=(
        "$LOCAL_SDK"
        "$HOME/urho3d-local"
        "$HOME/urho3d-build/out"
        "$HOME/urho3d_build/out"
        "$HOME/urho3d/build"
        "$HOME/urho3d_build"
        "/opt/homebrew"
        "/usr/local"
        "/usr"
    )

    if [ -n "${URHO_INC_OVERRIDE:-}" ]; then
        local ov_inc ov_prefix
        ov_inc="$(find_urho_inc_in "$URHO_INC_OVERRIDE" || true)"
        if [ -n "$ov_inc" ]; then
            ov_prefix="$(dirname "$ov_inc")"
            for ld in "$ov_prefix/lib" "$ov_prefix/lib/Urho3D" "$ov_prefix/lib64" "$ov_prefix/lib64/Urho3D"; do
                if find_urho_lib_in "$ld" >/dev/null && urho_lib_is_usable "$ld"; then
                    URHO_INC="$ov_inc"
                    URHO_LIBDIR="$ld"
                    return 0
                fi
            done
        fi
    fi

    for p in "${prefixes[@]}"; do
        [ -n "$p" ] && [ -d "$p" ] || continue
        local cand_inc="" cand_lib=""
        for id in "$p/include" "$p/include/Urho3D"; do
            cand_inc="$(find_urho_inc_in "$id" || true)"
            [ -n "$cand_inc" ] && break
        done
        [ -n "$cand_inc" ] || continue
        for ld in \
            "$p/lib" "$p/lib/Urho3D" "$p/lib64" "$p/lib64/Urho3D" \
            "$p/lib/x86_64-linux-gnu" "$p/lib/aarch64-linux-gnu"
        do
            cand_lib="$(find_urho_lib_in "$ld" || true)"
            [ -n "$cand_lib" ] && break
        done
        [ -n "$cand_lib" ] || continue
        if urho_lib_is_usable "$cand_lib"; then
            URHO_INC="$cand_inc"
            URHO_LIBDIR="$cand_lib"
            return 0
        else
            echo "[Urho3D] ВНИМАНИЕ: найденный SDK ($cand_lib) собран без видеодрайверов SDL (X11/Wayland) и выдаст 'No available video device'!" >&2
            echo "         Пропускаю его — будет использован или собран полноценный рабочий SDK." >&2
        fi
    done
}

URHO_INC_OVERRIDE="${URHO_INC:-}"
find_urho3d

if [ -n "$URHO_INC" ] && [ -n "$URHO_LIBDIR" ] && [ -n "${URHO3D_HOME:-}" ]; then
    mkdir -p "$ROOT/build"
    printf '%s\n' "$URHO3D_HOME" > "$ROOT/build/.urho_sdk_home" 2>/dev/null || true
elif [ -n "$URHO_INC" ] && [ -n "$URHO_LIBDIR" ] && { [ "$URHO_LIBDIR" = "$LOCAL_SDK/lib" ] || [ "$URHO_LIBDIR" = "$LOCAL_SDK/lib64" ]; }; then
    rm -f "$ROOT/build/.urho_sdk_home" 2>/dev/null || true
fi

if [ -z "$URHO_INC" ] || [ -z "$URHO_LIBDIR" ]; then
    # Перед сборкой Urho3D с нуля на Linux проверяем X11/XRandR/Audio заголовки, чтобы SDL внутри Urho3D не собрался с dummy-драйверами!
    if [ "$OS_NAME" = "Linux" ] && [ "$IS_NIX" -eq 0 ] && [ -n "${CXX:-}" ]; then
        sdk_missing=()
        has_cpp_header "X11/Xlib.h" || sdk_missing+=("libx11-dev")
        has_cpp_header "X11/extensions/Xrandr.h" || sdk_missing+=("libxrandr-dev")
        has_cpp_header "X11/extensions/shape.h" || sdk_missing+=("libxext-dev")
        has_cpp_header "GL/gl.h" || sdk_missing+=("libgl-dev")
        if ! has_cpp_header "alsa/asoundlib.h" && ! has_cpp_header "pulse/simple.h"; then
            sdk_missing+=("libasound2-dev/libpulse-dev")
        fi
        if [ "${#sdk_missing[@]}" -gt 0 ]; then
            PKG_MGR="$(detect_pkg_mgr)"
            CMD="$(install_cmd_for_mgr "$PKG_MGR")"
            echo "[Urho3D] Для сборки рабочего Urho3D SDK со звуком и X11/мультимонитором не хватает: ${sdk_missing[*]}"
            if [ "$INSTALL_DEPS" -eq 0 ] && [ -t 0 ] && [ -n "$CMD" ]; then
                printf "Установить системные пакеты автоматически? [Y/n]: "
                read -r ans || ans="n"
                case "${ans:-y}" in
                    [YyДд]*|"") INSTALL_DEPS=1 ;;
                esac
            fi
            if [ "$INSTALL_DEPS" -eq 1 ] && [ -n "$CMD" ]; then
                bash -c "$CMD"
            elif [ -n "$CMD" ]; then
                echo "ОШИБКА: Выполните сначала установку системных dev-пакетов или запустите с --install-deps:"
                echo "  $CMD"
                exit 1
            fi
        fi
    fi
    echo "================================================================"
    echo "[Urho3D] Локальный рабочий SDK не найден."
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

    # Патчим Urho3D 1.9.0 для совместимости с macOS Apple Silicon (arm64) и новым Apple SDK
    if [ "$OS_NAME" = "Darwin" ] && [ ! -f "$URHO_SRC/.chikengun_apple_patched_v4" ] && command -v python3 >/dev/null 2>&1; then
        python3 - "$URHO_SRC" << 'PYPATCH'
import pathlib, sys
root = pathlib.Path(sys.argv[1])

cct = root / "cmake/Modules/CheckCompilerToolchain.cmake"
if cct.exists():
    s = cct.read_text()
    s = s.replace('check_native_define ("__(arm|aarch64)__" ARM)', 'check_native_define ("__(arm|arm64|aarch64)__" ARM)')
    cct.write_text(s)

uc = root / "cmake/Modules/UrhoCommon.cmake"
if uc.exists():
    s = uc.read_text()
    s = s.replace('if (ARM AND CMAKE_SYSTEM_NAME STREQUAL Linux)', 'if (ARM AND (CMAKE_SYSTEM_NAME STREQUAL Linux OR CMAKE_SYSTEM_NAME STREQUAL Darwin))')
    s = s.replace('if (URHO3D_64BIT)\n                    # aarch64 has only one valid arch so far\n                    set (ARM_CFLAGS "${ARM_CFLAGS} -march=armv8-a")',
                  'if (URHO3D_64BIT AND NOT APPLE)\n                    # aarch64 has only one valid arch so far\n                    set (ARM_CFLAGS "${ARM_CFLAGS} -march=armv8-a")')
    s = s.replace('        elseif (APPLE)\n            if (ARM)', '        elseif (APPLE)\n            if (IOS OR TVOS)')
    uc.write_text(s)

su = root / "Source/Urho3D/CMakeLists.txt"
if su.exists():
    s = su.read_text()
    s = s.replace('if (APPLE AND NOT ARM)', 'if (APPLE AND NOT IOS AND NOT TVOS)')
    su.write_text(s)

sdlc = root / "Source/ThirdParty/SDL/CMakeLists.txt"
if sdlc.exists():
    s = sdlc.read_text()
    s = s.replace('if(HAVE_GCC_WERROR_DECLARATION_AFTER_STATEMENT AND NOT ANDROID)', 'if(FALSE)')
    sdlc.write_text(s)

for rel in ["Source/ThirdParty/FreeType/src/gzip/ftzconf.h", "Source/ThirdParty/FreeType/src/gzip/zconf.h"]:
    p = root / rel
    if p.exists():
        s = p.read_text()
        s = s.replace('#if !defined(MACOS) && !defined(TARGET_OS_MAC)',
                      '#if !defined(MACOS) && (!defined(TARGET_OS_MAC) || (!defined(__MACTYPES__) && !defined(_MACTYPES_)))')
        p.write_text(s)

fc = root / "Source/Urho3D/IO/File.cpp"
if fc.exists():
    s = fc.read_text()
    s = s.replace('#ifdef _MSC_VER\r\n    return _fseeki64(stream, offset, origin);\r\n#else',
                  '#ifdef _MSC_VER\n    return _fseeki64(stream, offset, origin);\n#elif defined(__APPLE__)\n    return fseeko(stream, offset, origin);\n#else')
    s = s.replace('#ifdef _MSC_VER\n    return _fseeki64(stream, offset, origin);\n#else',
                  '#ifdef _MSC_VER\n    return _fseeki64(stream, offset, origin);\n#elif defined(__APPLE__)\n    return fseeko(stream, offset, origin);\n#else')
    s = s.replace('#ifdef _MSC_VER\r\n    return _ftelli64(stream);\r\n#else',
                  '#ifdef _MSC_VER\n    return _ftelli64(stream);\n#elif defined(__APPLE__)\n    return ftello(stream);\n#else')
    s = s.replace('#ifdef _MSC_VER\n    return _ftelli64(stream);\n#else',
                  '#ifdef _MSC_VER\n    return _ftelli64(stream);\n#elif defined(__APPLE__)\n    return ftello(stream);\n#else')
    s = s.replace('handle_ = fopen64(GetNativePath(fileName).CString(), openMode[mode]);',
                  '#ifdef __APPLE__\n    handle_ = fopen(GetNativePath(fileName).CString(), openMode[mode]);\n#else\n    handle_ = fopen64(GetNativePath(fileName).CString(), openMode[mode]);\n#endif')
    s = s.replace('handle_ = fopen64(GetNativePath(fileName).CString(), openMode[mode + 1]);',
                  '#ifdef __APPLE__\n        handle_ = fopen(GetNativePath(fileName).CString(), openMode[mode + 1]);\n#else\n        handle_ = fopen64(GetNativePath(fileName).CString(), openMode[mode + 1]);\n#endif')
    fc.write_text(s)

(root / ".chikengun_apple_patched_v4").write_text("1\n")
PYPATCH
    fi
    rm -rf "$URHO_BLD"

    CMAKE_EXTRA_ARGS=(
        "-DLIB_SUFFIX="
        "-DURHO3D_USE_LIB_DEB=1"
        "-DURHO3D_DEPLOYMENT_TARGET=generic"
        "-DCMAKE_C_FLAGS=-std=gnu11 -Wno-error -Wno-implicit-function-declaration -Wno-incompatible-pointer-types"
        "-DCMAKE_CXX_FLAGS=-include cstdint -Wno-error"
    )
    if [ "$OS_NAME" = "Darwin" ]; then
        CMAKE_EXTRA_ARGS+=("-DVIDEO_X11=0")
        if [ "$(uname -m)" = "arm64" ]; then
            CMAKE_EXTRA_ARGS+=("-DARM=1" "-DX86=0")
        fi
    elif [ "$OS_NAME" = "Linux" ]; then
        CMAKE_EXTRA_ARGS+=("-DVIDEO_X11=1" "-DALSA=1" "-DPULSEAUDIO=1")
        if command -v wayland-scanner >/dev/null 2>&1 && command -v pkg-config >/dev/null 2>&1 && \
           pkg-config --exists wayland-client wayland-egl wayland-cursor xkbcommon 2>/dev/null; then
            CMAKE_EXTRA_ARGS+=("-DVIDEO_WAYLAND=1")
        fi
    fi
    # CMake >= 3.27 / 4.0 отклоняет старые ThirdParty cmake_minimum_required без policy minimum
    CMAKE_VER="$(cmake --version 2>/dev/null | head -n1 | sed -E 's/[^0-9]*([0-9]+\.[0-9]+).*/\1/' || echo "3.20")"
    CMAKE_MAJ="${CMAKE_VER%%.*}"
    CMAKE_MIN="${CMAKE_VER#*.}"; CMAKE_MIN="${CMAKE_MIN%%.*}"
    if [ "${CMAKE_MAJ:-0}" -gt 3 ] || { [ "${CMAKE_MAJ:-0}" -eq 3 ] && [ "${CMAKE_MIN:-0}" -ge 27 ]; }; then
        CMAKE_EXTRA_ARGS+=("-DCMAKE_POLICY_VERSION_MINIMUM=3.5")
    fi
    if [ "$IS_WIN" -eq 1 ]; then
        CMAKE_EXTRA_ARGS+=("-DURHO3D_OPENGL=1" "-DURHO3D_D3D11=0" "-DURHO3D_WIN32_CONSOLE=0")
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
    rm -rf "$URHO_BLD" "$ROOT/build/.urho_sdk_home"
    find_urho3d
fi

if [ -z "$URHO_INC" ] || [ -z "$URHO_LIBDIR" ]; then
    echo "ОШИБКА: Не удалось найти или собрать Urho3D SDK."
    exit 1
fi
echo "[Urho3D] Заголовки:  $URHO_INC"
echo "[Urho3D] Библиотеки: $URHO_LIBDIR"
if [ "${#NIX_LINK_FLAGS[@]}" -gt 0 ]; then
    echo "[Nix] Пути линковки: ${NIX_LINK_FLAGS[*]}"
fi

# ---- 3. Флаги SDL2, Bullet3, статического Urho3D и системных библиотек ----
SDL_INC=""
SDL_INC2=""
SDL_LIBS=""

if [ -d "$URHO_INC/Urho3D/ThirdParty/SDL" ] && [ -f "$URHO_INC/Urho3D/ThirdParty/SDL/SDL_joystick.h" ]; then
    SDL_INC="$URHO_INC/Urho3D/ThirdParty"
fi

IS_STATIC_URHO=0
if [ -f "$URHO_LIBDIR/libUrho3D.a" ] && [ ! -f "$URHO_LIBDIR/libUrho3D.so" ] && [ ! -f "$URHO_LIBDIR/libUrho3D.dylib" ] && [ ! -f "$URHO_LIBDIR/libUrho3D.dll.a" ]; then
    IS_STATIC_URHO=1
fi

NEED_SYS_SDL=0
if [ "$IS_STATIC_URHO" -eq 0 ] || [ -z "$SDL_INC" ]; then
    NEED_SYS_SDL=1
elif command -v nm >/dev/null 2>&1 && ! nm -g --defined-only "$URHO_LIBDIR/libUrho3D.a" 2>/dev/null | grep -qE ' T _?SDL_Init$'; then
    NEED_SYS_SDL=1
fi

if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists sdl2 2>/dev/null; then
    if [ "$NEED_SYS_SDL" -eq 1 ]; then
        SDL_LIBS="$(pkg-config --libs sdl2 2>/dev/null || true)"
    fi
    for f in $(pkg-config --cflags-only-I sdl2 2>/dev/null | tr ' ' '\n' | sed 's/^-I//'); do
        if [ -z "$SDL_INC" ] && [ -f "$f/SDL/SDL_joystick.h" ]; then SDL_INC="$f"; break; fi
        if [ -f "$f/SDL2/SDL_joystick.h" ]; then SDL_INC2="$f/SDL2"; fi
        if [ -f "$f/SDL_joystick.h" ] && [ "$(basename "$f")" = "SDL2" ]; then SDL_INC2="$f"; fi
    done
elif [ "$NEED_SYS_SDL" -eq 1 ] && can_link_lib SDL2; then
    SDL_LIBS="-lSDL2"
fi

if [ -z "$SDL_INC" ]; then
    for d in \
        "$URHO_INC/Urho3D/ThirdParty" \
        "$HOME/.nix-profile/include" \
        "/run/current-system/sw/include" \
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

# Фикс для неустановленных сборок Urho3D (например ~/urho3d/build):
# SDL_joystick.h лежит по симлинку в Source/ThirdParty/SDL/include, а SDL_config.h — в generated/.
if [ -n "$SDL_INC" ] && [ ! -f "$SDL_INC/SDL/SDL_config.h" ]; then
    SDL_CFG_DIR=""
    urho_inc_parent="$(dirname "$URHO_INC")"
    urho_lib_parent="$(dirname "$URHO_LIBDIR")"
    sdl_cfg_candidates=(
        "$urho_inc_parent/Source/ThirdParty/SDL/include/generated"
        "$urho_inc_parent/Source/ThirdParty/SDL/include"
        "$urho_lib_parent/Source/ThirdParty/SDL/include/generated"
        "$urho_lib_parent/Source/ThirdParty/SDL/include"
        "${URHO3D_HOME:-}/Source/ThirdParty/SDL/include/generated"
        "${URHO3D_HOME:-}/build/Source/ThirdParty/SDL/include/generated"
        "$URHO_INC/Urho3D/ThirdParty/SDL"
    )
    if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists sdl2 2>/dev/null; then
        for f in $(pkg-config --cflags-only-I sdl2 2>/dev/null | tr ' ' '\n' | sed 's/^-I//'); do
            [ -n "$f" ] && sdl_cfg_candidates+=("$f" "$f/SDL2")
        done
    fi
    sdl_cfg_candidates+=(
        "$HOME/.nix-profile/include/SDL2"
        "/run/current-system/sw/include/SDL2"
        "/opt/homebrew/include/SDL2"
        "/usr/local/include/SDL2"
        "/usr/include/SDL2"
    )
    for scd in "${sdl_cfg_candidates[@]}"; do
        [ -n "$scd" ] || continue
        if [ -f "$scd/SDL_config.h" ]; then
            SDL_CFG_DIR="$scd"
            break
        fi
    done
    if [ -n "$SDL_CFG_DIR" ]; then
        SDL_CFLAGS="$SDL_CFLAGS -isystem $SDL_CFG_DIR"
        [ -d "$SDL_INC/SDL" ] && SDL_CFLAGS="$SDL_CFLAGS -isystem $SDL_INC/SDL"
    fi
fi

BULLET_CFLAGS=""
for d in \
    "$URHO_INC/Urho3D/ThirdParty/Bullet" \
    "$URHO_INC/Urho3D/ThirdParty/Bullet/src" \
    "${URHO3D_HOME:-}/include/Urho3D/ThirdParty/Bullet" \
    "$HOME/urho3d-local/include/Urho3D/ThirdParty/Bullet" \
    "$HOME/urho3d-build/out/include/Urho3D/ThirdParty/Bullet" \
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
URHO_PC_DEFINES=()
URHO_PC_EXTRA_LIBS=()

if [ "$IS_STATIC_URHO" -eq 1 ]; then
    EXTRA_DEFINES+=("-DURHO3D_STATIC_DEFINE")
fi

# Если есть Urho3D.pc для выбранного URHO_LIBDIR, подтягиваем его -D дефайны и системные -l библиотеки
if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists Urho3D 2>/dev/null; then
    PC_LIBDIR="$(pkg-config --variable=libdir Urho3D 2>/dev/null || true)"
    for tok in $(pkg-config --cflags Urho3D 2>/dev/null || true); do
        case "$tok" in
            -D*)
                if [ "$tok" != "-DURHO3D_STATIC_DEFINE" ] || [ "$IS_STATIC_URHO" -eq 1 ]; then
                    append_unique_word URHO_PC_DEFINES "$tok"
                fi
                ;;
        esac
    done
    if [ -z "$PC_LIBDIR" ] || [ "$PC_LIBDIR" = "$URHO_LIBDIR" ] || [ "$(cd "$PC_LIBDIR" 2>/dev/null && pwd -P)" = "$(cd "$URHO_LIBDIR" 2>/dev/null && pwd -P)" ]; then
        for tok in $(pkg-config --libs Urho3D 2>/dev/null || true); do
            case "$tok" in
                -lUrho3D|-L*) ;;
                -l*|-pthread|-Wl,*)
                    URHO_PC_EXTRA_LIBS+=("$tok")
                    ;;
            esac
        done
    else
        for tok in $(pkg-config --libs Urho3D 2>/dev/null || true); do
            case "$tok" in
                -lUrho3D|-L*) ;;
                -l*|-pthread)
                    URHO_PC_EXTRA_LIBS+=("$tok")
                    ;;
            esac
        done
    fi
fi

if [ "$OS_NAME" = "Linux" ]; then
    RPATH_FLAGS+=("-Wl,-rpath,$URHO_LIBDIR" "-Wl,-rpath,\$ORIGIN/lib")
    for rd in ${NIX_RPATH_DIRS[@]+"${NIX_RPATH_DIRS[@]}"}; do
        RPATH_FLAGS+=("-Wl,-rpath,$rd")
    done
    if [ "$IS_STATIC_URHO" -eq 1 ]; then
        # Сначала пробуем pkg-config для NixOS / нестандартных путей, затем probe через линковщик
        if command -v pkg-config >/dev/null 2>&1; then
            for pc_mod in \
                gl glu opengl glx x11 xext xrandr xi \
                xcursor xinerama xxf86vm xrender xfixes xscrnsaver xcb \
                alsa libpulse libpulse-simple libudev dbus-1 xkbcommon \
                wayland-client wayland-egl wayland-cursor
            do
                if pkg-config --exists "$pc_mod" 2>/dev/null; then
                    for tok in $(pkg-config --libs "$pc_mod" 2>/dev/null || true); do
                        case "$tok" in
                            -L*|-l*|-pthread|-Wl,*)
                                append_unique_word EXTRA_SYS_LIBS "$tok"
                                ;;
                        esac
                    done
                fi
            done
        fi
        for syslib in \
            GL OpenGL GLX GLU \
            X11 Xext Xrandr Xi Xcursor Xinerama Xxf86vm Xrender Xfixes Xss xcb \
            asound pulse pulse-simple udev dbus-1 xkbcommon \
            wayland-client wayland-egl wayland-cursor \
            rt dl m
        do
            if can_link_lib "$syslib"; then
                append_unique_word EXTRA_SYS_LIBS "-l$syslib"
            fi
        done
    else
        for syslib in GL X11 rt dl m; do
            can_link_lib "$syslib" && append_unique_word EXTRA_SYS_LIBS "-l$syslib"
        done
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
    "$HOME/urho3d-local/share/Urho3D/Resources/CoreData" \
    "$HOME/urho3d-local/share/Urho3D/CoreData" \
    "$HOME/urho3d-build/out/share/Urho3D/Resources/CoreData" \
    "$HOME/urho3d-build/bin/CoreData" \
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
    "$HOME/urho3d-local/share/Urho3D/Resources/Data" \
    "$HOME/urho3d-local/share/Urho3D/Data" \
    "$HOME/urho3d-build/out/share/Urho3D/Resources/Data" \
    "$HOME/urho3d-build/bin/Data" \
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

# ---- 5. Валидация и генерация 13 PBR наборов текстур (вкл. банные материалы) ----
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
    "BanyaPlankDiffuse.png", "BanyaPlankNormal.png", "BanyaPlankRoughMetal.png",
    "BanyaStoneDiffuse.png", "BanyaStoneNormal.png", "BanyaStoneRoughMetal.png",
    "BanyaCopperDiffuse.png", "BanyaCopperNormal.png", "BanyaCopperRoughMetal.png",
    "BanyaBirchDiffuse.png", "BanyaBirchNormal.png", "BanyaBirchRoughMetal.png",
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
    ${EXTRA_DEFINES[@]+"${EXTRA_DEFINES[@]}"} \
    ${URHO_PC_DEFINES[@]+"${URHO_PC_DEFINES[@]}"} \
    -isystem "$URHO_INC" \
    -isystem "$URHO_INC/Urho3D/ThirdParty" \
    $SDL_CFLAGS \
    $BULLET_CFLAGS \
    ${NIX_INC_FLAGS[@]+"${NIX_INC_FLAGS[@]}"} \
    -L"$URHO_LIBDIR" \
    ${NIX_LINK_FLAGS[@]+"${NIX_LINK_FLAGS[@]}"} \
    ${RPATH_FLAGS[@]+"${RPATH_FLAGS[@]}"} \
    -lUrho3D \
    ${BULLET_LIBS[@]+"${BULLET_LIBS[@]}"} \
    $SDL_LIBS \
    ${URHO_PC_EXTRA_LIBS[@]+"${URHO_PC_EXTRA_LIBS[@]}"} \
    ${EXTRA_SYS_LIBS[@]+"${EXTRA_SYS_LIBS[@]}"} \
    -lpthread

echo "================================================================"
echo "Готово! Нативный клиент собран: build/MyShooter${EXE_EXT}"
echo "  Запуск одной командой:   ./run.sh"
echo "  В 4K качестве:           ./build.sh --4k && ./run.sh"
if [ "$OS_NAME" = "Linux" ]; then
    echo "  Портативный релиз Linux: ./tools/package_linux.sh"
fi
echo "================================================================"
