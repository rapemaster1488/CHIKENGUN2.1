#!/usr/bin/env bash
# Configure tiny fake SDKs, without building the game, to test SDL link choices
# and ensure pkg-config flags come from the selected Urho3D installation.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cmake="$1"
cxx="$2"
pkg_config="$3"
ar="${4:-ar}"
tmp="$(mktemp -d)"
cleanup() {
    local status=$?
    if [ "$status" -ne 0 ]; then
        echo 'CMake SDL linkage regression test failed.' >&2
        for log in "$tmp"/*/configure.log; do
            [ ! -f "$log" ] || { echo "--- $log ---" >&2; tail -40 "$log" >&2; }
        done
    fi
    rm -rf "$tmp"
}
trap cleanup EXIT

mkdir -p "$tmp/modules" "$tmp/sdl/lib/pkgconfig" "$tmp/sdl/include"
# The test verifies link decisions, not OpenGL's own system discovery.
cat > "$tmp/modules/FindOpenGL.cmake" <<'FIND_GL'
set(OpenGL_FOUND TRUE)
if(NOT TARGET OpenGL::GL)
    add_library(OpenGL::GL INTERFACE IMPORTED)
endif()
FIND_GL
cat > "$tmp/sdl/lib/pkgconfig/sdl2.pc" <<EOF
prefix=$tmp/sdl
libdir=\${prefix}/lib
includedir=\${prefix}/include
Name: SDL2
Description: synthetic SDL used by CMake linkage regression tests
Version: 2.0.10
Libs: -L\${libdir} -lSDL2
Cflags: -I\${includedir}
EOF

make_sdk() {
    local name="$1" define="$2" sdk="$tmp/$1-sdk"
    mkdir -p "$sdk/include/Urho3D/Engine" "$sdk/include/Urho3D/ThirdParty/SDL" "$sdk/lib/Urho3D" "$sdk/lib/pkgconfig"
    : > "$sdk/include/Urho3D/Engine/Application.h"
    : > "$sdk/include/Urho3D/ThirdParty/SDL/SDL_joystick.h"
    cat > "$sdk/lib/pkgconfig/Urho3D.pc" <<EOF
prefix=$sdk
libdir=\${prefix}/lib/Urho3D
includedir=\${prefix}/include
Name: Urho3D
Description: synthetic Urho3D used by CMake linkage regression tests
Version: 1.9.0
Libs: -L\${libdir} -lUrho3D
Cflags: -I\${includedir} -D$define
EOF
    local source='extern "C" int urho_without_exported_sdl() { return 0; }'
    case "$name" in bundled|static-bundled) source='extern "C" int SDL_Init(unsigned) { return 0; }' ;; esac
    if [[ "$name" = static-* ]]; then
        printf '%s\n' "$source" | "$cxx" -x c++ -c -o "$tmp/$name.o" -
        "$ar" rcs "$sdk/lib/Urho3D/libUrho3D.a" "$tmp/$name.o"
    else
        printf '%s\n' "$source" | "$cxx" -x c++ -shared -fPIC -o "$sdk/lib/Urho3D/libUrho3D.so" -
    fi
}
make_sdk bundled SELECTED_SDK_PC
make_sdk external EXTERNAL_SDK_PC
make_sdk wrong WRONG_SDK_PC
make_sdk static-bundled STATIC_BUNDLED_PC
make_sdk static-external STATIC_EXTERNAL_PC

configure() {
    local name="$1" sdk="$2"
    shift 2
    mkdir -p "$tmp/$name"
    # Limit pkg-config to the synthetic SDL; the project adds the selected
    # Urho3D .pc directory itself. No installed SDL or Urho3D is required.
    env -u URHO3D_HOME PKG_CONFIG_PATH="$tmp/sdl/lib/pkgconfig" PKG_CONFIG_LIBDIR="$tmp/sdl/lib/pkgconfig" \
        "$cmake" -S "$root" -B "$tmp/$name" -G 'Unix Makefiles' \
        -DBUILD_GAME=ON -DAUTO_FETCH_URHO3D=OFF -DCMAKE_DISABLE_FIND_PACKAGE_X11=TRUE \
        "-DCMAKE_MODULE_PATH=$tmp/modules" "-DCMAKE_CXX_COMPILER=$cxx" "-DPKG_CONFIG_EXECUTABLE=$pkg_config" \
        "-DURHO3D_HOME=$sdk" "$@" > "$tmp/$name/configure.log" 2>&1
}

configure bundled "$tmp/bundled-sdk"
link="$tmp/bundled/CMakeFiles/MyShooter.dir/link.txt"
flags="$tmp/bundled/CMakeFiles/MyShooter.dir/flags.make"
grep -Fq -- '-DSELECTED_SDK_PC' "$flags"
if grep -Eq '(^|[[:space:]])-lSDL2([[:space:]]|$)' "$link"; then
    echo 'A shared SDK exporting SDL must not be linked to a second SDL.' >&2
    cat "$link" >&2
    exit 1
fi

configure external "$tmp/external-sdk"
link="$tmp/external/CMakeFiles/MyShooter.dir/link.txt"
grep -Fq -- '-DEXTERNAL_SDK_PC' "$tmp/external/CMakeFiles/MyShooter.dir/flags.make"
grep -Eq '(^|[[:space:]])-lSDL2([[:space:]]|$)' "$link" || {
    echo 'A shared SDK without exported SDL needs explicit SDL2 linkage.' >&2
    cat "$link" >&2
    exit 1
}

configure static-bundled "$tmp/static-bundled-sdk"
link="$tmp/static-bundled/CMakeFiles/MyShooter.dir/link.txt"
if grep -Eq '(^|[[:space:]])-lSDL2([[:space:]]|$)' "$link"; then
    echo 'A static SDK containing SDL must not link another SDL.' >&2
    cat "$link" >&2
    exit 1
fi

# Some SDKs retain the bundled headers but use a separately linked SDL.
configure static-external "$tmp/static-external-sdk"
link="$tmp/static-external/CMakeFiles/MyShooter.dir/link.txt"
grep -Eq '(^|[[:space:]])-lSDL2([[:space:]]|$)' "$link" || {
    echo 'A static SDK without SDL symbols needs explicit SDL2 linkage.' >&2
    cat "$link" >&2
    exit 1
}

# An uninstalled Urho build has its config in generated/, while SDL_platform.h
# is beside SDL_joystick.h. Both directories must reach the compiler.
mkdir -p "$tmp/bundled-sdk/Source/ThirdParty/SDL/include/generated"
: > "$tmp/bundled-sdk/Source/ThirdParty/SDL/include/generated/SDL_config.h"
configure generated-config "$tmp/bundled-sdk"
flags="$tmp/generated-config/CMakeFiles/MyShooter.dir/flags.make"
grep -Fq "$tmp/bundled-sdk/include/../Source/ThirdParty/SDL/include/generated" "$flags"
grep -Fq "$tmp/bundled-sdk/include/Urho3D/ThirdParty/SDL" "$flags"

# Explicit SDKs must take precedence even when CMAKE_PREFIX_PATH points to a
# different SDK. Check both the CMake variable and environment form.
configure explicit "$tmp/bundled-sdk" "-DCMAKE_PREFIX_PATH=$tmp/wrong-sdk"
grep -Fq "URHO3D_LIBRARY:FILEPATH=$tmp/bundled-sdk/lib/Urho3D/libUrho3D.so" "$tmp/explicit/CMakeCache.txt"
grep -Fq "URHO3D_INCLUDE_DIR:PATH=$tmp/bundled-sdk/include" "$tmp/explicit/CMakeCache.txt"
grep -Fq -- '-DSELECTED_SDK_PC' "$tmp/explicit/CMakeFiles/MyShooter.dir/flags.make"

mkdir -p "$tmp/environment"
env URHO3D_HOME="$tmp/bundled-sdk" PKG_CONFIG_PATH="$tmp/sdl/lib/pkgconfig" PKG_CONFIG_LIBDIR="$tmp/sdl/lib/pkgconfig" \
    "$cmake" -S "$root" -B "$tmp/environment" -G 'Unix Makefiles' \
    -DBUILD_GAME=ON -DAUTO_FETCH_URHO3D=OFF -DCMAKE_DISABLE_FIND_PACKAGE_X11=TRUE \
    "-DCMAKE_MODULE_PATH=$tmp/modules" "-DCMAKE_CXX_COMPILER=$cxx" "-DPKG_CONFIG_EXECUTABLE=$pkg_config" \
    "-DCMAKE_PREFIX_PATH=$tmp/wrong-sdk" > "$tmp/environment/configure.log" 2>&1
grep -Fq "URHO3D_LIBRARY:FILEPATH=$tmp/bundled-sdk/lib/Urho3D/libUrho3D.so" "$tmp/environment/CMakeCache.txt"
grep -Fq "URHO3D_INCLUDE_DIR:PATH=$tmp/bundled-sdk/include" "$tmp/environment/CMakeCache.txt"
grep -Fq -- '-DSELECTED_SDK_PC' "$tmp/environment/CMakeFiles/MyShooter.dir/flags.make"

# Verify the common multiarch layout when CMake detected an architecture.
multiarch="$(sed -n 's/^set(CMAKE_CXX_LIBRARY_ARCHITECTURE "\([^"]*\)")$/\1/p' "$tmp"/bundled/CMakeFiles/*/CMakeCXXCompiler.cmake)"
if [ -n "$multiarch" ]; then
    mkdir -p "$tmp/multiarch-sdk/include/Urho3D/Engine" "$tmp/multiarch-sdk/lib/$multiarch/Urho3D"
    : > "$tmp/multiarch-sdk/include/Urho3D/Engine/Application.h"
    cp "$tmp/bundled-sdk/lib/Urho3D/libUrho3D.so" "$tmp/multiarch-sdk/lib/$multiarch/Urho3D/"
    configure multiarch "$tmp/multiarch-sdk"
    grep -Fq "URHO3D_LIBRARY:FILEPATH=$tmp/multiarch-sdk/lib/$multiarch/Urho3D/libUrho3D.so" "$tmp/multiarch/CMakeCache.txt"
fi

# Do not silently combine an incomplete explicit SDK with another SDK that
# happens to be installed or on CMAKE_PREFIX_PATH.
mkdir -p "$tmp/headers-only-sdk/include/Urho3D/Engine" "$tmp/library-only-sdk/lib/Urho3D"
: > "$tmp/headers-only-sdk/include/Urho3D/Engine/Application.h"
cp "$tmp/bundled-sdk/lib/Urho3D/libUrho3D.so" "$tmp/library-only-sdk/lib/Urho3D/"
for incomplete in headers-only library-only; do
    if configure "$incomplete" "$tmp/$incomplete-sdk" "-DCMAKE_PREFIX_PATH=$tmp/wrong-sdk"; then
        echo "CMake accepted an incomplete explicit SDK: $incomplete" >&2
        exit 1
    fi
    grep -Fq 'URHO3D_HOME does not contain' "$tmp/$incomplete/configure.log"
done

# Even an explicitly specified library/include pair can differ from the .pc
# found first in URHO3D_HOME; do not mix flags or libs from those two SDKs.
configure mismatch "$tmp/wrong-sdk" \
    "-DURHO3D_INCLUDE_DIR=$tmp/bundled-sdk/include" \
    "-DURHO3D_LIBRARY=$tmp/bundled-sdk/lib/Urho3D/libUrho3D.so"
grep -Fq 'Ignoring Urho3D.pc' "$tmp/mismatch/configure.log"
if grep -Fq -- '-DWRONG_SDK_PC' "$tmp/mismatch/CMakeFiles/MyShooter.dir/flags.make" || \
   grep -Fq -- "$tmp/wrong-sdk/lib/Urho3D" "$tmp/mismatch/CMakeFiles/MyShooter.dir/link.txt"; then
    echo 'CMake mixed a selected SDK with a different Urho3D.pc.' >&2
    exit 1
fi

echo 'CMake static/shared SDL linkage and Urho3D SDK selection passed.'
