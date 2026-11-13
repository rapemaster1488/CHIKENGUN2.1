#!/usr/bin/env bash
# Exercise the launch/build flags and SDK rollback in an isolated fake project.
# No SDL, Urho3D, display server, network, or installed SDK is needed.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
export HOME="$tmp/home"
unset URHO3D_HOME URHO_INC URHO3D_ALLOW_HEADLESS IN_NIX_SHELL NIX_CC NIX_CFLAGS_COMPILE NIX_LDFLAGS NIX_PROFILES LD_LIBRARY_PATH SDL_VIDEODRIVER || true
export AUTO_INSTALL_DEPS=0
# /usr/bin/env need not exist inside a Nix build sandbox.
bash_path="$(command -v bash)"
export bash_path
mkdir -p "$HOME" "$tmp/launcher"
cp "$root/run.sh" "$tmp/launcher/run.sh"

# A fake build produces a fake client that records only the application flags.
printf '#!%s\n' "$bash_path" > "$tmp/launcher/build.sh"
cat >> "$tmp/launcher/build.sh" <<'BUILD'
set -euo pipefail
printf '%s\n' "$@" > "$BUILD_TRACE"
printf '%s\n' "${URHO3D_HOME:-<unset>}" > "$BUILD_SDK_TRACE"
mkdir -p build/Data/Textures build/CoreData/Shaders
touch build/Data/Textures/Skybox4K.xml build/Data/Textures/BanyaBirchDiffuse.png
printf '#!%s\n' "$bash_path" > build/MyShooter
cat >> build/MyShooter <<'APP'
printf '%s\n' "$@" > "$APP_TRACE"
printf '%s\n' "${LD_LIBRARY_PATH:-}" > "$APP_LIBRARY_PATH_TRACE"
printf '%s\n' "${SDL_VIDEODRIVER:-<unset>}" > "$APP_VIDEO_TRACE"
APP
chmod +x build/MyShooter
BUILD
chmod +x "$tmp/launcher/build.sh"
export BUILD_TRACE="$tmp/launcher-build-args" BUILD_SDK_TRACE="$tmp/launcher-build-sdk" \
    APP_TRACE="$tmp/launcher-app-args" APP_LIBRARY_PATH_TRACE="$tmp/launcher-library-path" APP_VIDEO_TRACE="$tmp/launcher-video"

"$bash_path" "$tmp/launcher/run.sh" --rebuild-urho3d -j4 --diagnose-sdl -nosound > /dev/null
printf '%s\n' --rebuild-urho3d -j4 > "$tmp/expected"
cmp "$tmp/expected" "$BUILD_TRACE"
printf '%s\n' --diagnose-sdl -nosound > "$tmp/expected"
cmp "$tmp/expected" "$APP_TRACE"
# An unset URHO3D_HOME must not accidentally inject /lib into the search path.
if tr ':' '\n' < "$APP_LIBRARY_PATH_TRACE" | grep -Fxq /lib; then
    echo 'Unset URHO3D_HOME unexpectedly added /lib to LD_LIBRARY_PATH' >&2
    exit 1
fi

mkdir -p "$tmp/external-sdk/lib/Urho3D" "$tmp/launcher/.cache/urho3d-sdk/lib/Urho3D" \
    "$HOME/urho3d-local/lib" "$tmp/user-runtime"
URHO3D_HOME="$tmp/external-sdk" LD_LIBRARY_PATH="$tmp/user-runtime" \
    "$bash_path" "$tmp/launcher/run.sh" --diagnose-sdl > /dev/null
# Another SDK with the same SONAME must not shadow the explicit SDK at runtime.
external_order="$(tr ':' '\n' < "$APP_LIBRARY_PATH_TRACE" | grep -nFx "$tmp/external-sdk/lib/Urho3D" | cut -d: -f1)"
cache_order="$(tr ':' '\n' < "$APP_LIBRARY_PATH_TRACE" | grep -nFx "$tmp/launcher/.cache/urho3d-sdk/lib/Urho3D" | cut -d: -f1)"
home_order="$(tr ':' '\n' < "$APP_LIBRARY_PATH_TRACE" | grep -nFx "$HOME/urho3d-local/lib" | cut -d: -f1)"
[ "$external_order" -lt "$cache_order" ] && [ "$external_order" -lt "$home_order" ]
tr ':' '\n' < "$APP_LIBRARY_PATH_TRACE" | grep -Fxq "$tmp/user-runtime"

# Remember the actual linked library directory (including multiarch layouts),
# but never let a stale marker shadow a different SDK explicitly requested now.
mkdir -p "$tmp/multiarch-runtime/lib/test-linux-gnu/Urho3D"
printf '%s\n' "$tmp/multiarch-runtime/lib/test-linux-gnu/Urho3D" > "$tmp/launcher/build/.urho_libdir"
"$bash_path" "$tmp/launcher/run.sh" --diagnose-sdl > /dev/null
[ "$(cut -d: -f1 "$APP_LIBRARY_PATH_TRACE")" = "$tmp/multiarch-runtime/lib/test-linux-gnu/Urho3D" ]
URHO3D_HOME="$tmp/external-sdk" "$bash_path" "$tmp/launcher/run.sh" --diagnose-sdl > /dev/null
[ "$(cut -d: -f1 "$APP_LIBRARY_PATH_TRACE")" = "$tmp/external-sdk/lib" ]
if tr ':' '\n' < "$APP_LIBRARY_PATH_TRACE" | grep -Fxq "$tmp/multiarch-runtime/lib/test-linux-gnu/Urho3D"; then
    echo 'A stale runtime marker shadowed an explicitly selected SDK.' >&2
    exit 1
fi
printf '%s\n' "$tmp/external-sdk" > "$tmp/launcher/build/.urho_sdk_home"
"$bash_path" "$tmp/launcher/run.sh" --rebuild-urho3d --diagnose-sdl > /dev/null
[ "$(cat "$BUILD_SDK_TRACE")" = '<unset>' ]
rm -f "$tmp/launcher/build/.urho_sdk_home" "$tmp/launcher/build/.urho_libdir"

if [ "$(uname -s)" = Linux ]; then
    # Simulate a large nm output: grep must drain the pipe rather than closing
    # it early and misclassifying the linked SDL under set -o pipefail.
    mkdir -p "$tmp/launcher-tools"
    printf '#!%s\n' "$bash_path" > "$tmp/launcher-tools/nm"
    cat >> "$tmp/launcher-tools/nm" <<'NM'
printf '00000000 T SDL_Init\n'
for ((i=0; i<4000; ++i)); do printf '00000000 T symbol_%s\n' "$i"; done
[ "${MOCK_HEADLESS:-0}" = 1 ] || printf '00000000 D X11_bootstrap\n'
NM
    chmod +x "$tmp/launcher-tools/nm"
    rm -f "$BUILD_TRACE" "$APP_TRACE"
    env -u DISPLAY -u WAYLAND_DISPLAY PATH="$tmp/launcher-tools:$PATH" SDL_VIDEODRIVER=wayland \
        "$bash_path" "$tmp/launcher/run.sh" --diagnose-sdl > /dev/null
    [ ! -e "$BUILD_TRACE" ] && [ -e "$APP_TRACE" ] && [ "$(cat "$APP_VIDEO_TRACE")" = wayland ]
    rm -f "$BUILD_TRACE" "$APP_TRACE"
    env -u DISPLAY -u WAYLAND_DISPLAY PATH="$tmp/launcher-tools:$PATH" MOCK_HEADLESS=1 \
        "$bash_path" "$tmp/launcher/run.sh" --diagnose-sdl > /dev/null
    [ ! -e "$BUILD_TRACE" ] && [ -e "$APP_TRACE" ] # Diagnose the existing binary.
    env -u DISPLAY -u WAYLAND_DISPLAY PATH="$tmp/launcher-tools:$PATH" MOCK_HEADLESS=1 \
        "$bash_path" "$tmp/launcher/run.sh" > /dev/null
    [ -e "$BUILD_TRACE" ] # Ordinary launches rebuild a known headless binary.
fi

"$bash_path" "$tmp/launcher/run.sh" --rebuild-urho3d --jobs=5 --diagnose-sdl > /dev/null
printf '%s\n' --rebuild-urho3d --jobs=5 > "$tmp/expected"
cmp "$tmp/expected" "$BUILD_TRACE"
printf '%s\n' --diagnose-sdl > "$tmp/expected"
cmp "$tmp/expected" "$APP_TRACE"

"$bash_path" "$tmp/launcher/run.sh" --rebuild-urho3d --jobs 3 --diagnose-sdl > /dev/null
printf '%s\n' --rebuild-urho3d --jobs 3 > "$tmp/expected"
cmp "$tmp/expected" "$BUILD_TRACE"
"$bash_path" "$tmp/launcher/run.sh" --rebuild-urho3d -j 2 --diagnose-sdl > /dev/null
printf '%s\n' --rebuild-urho3d -j 2 > "$tmp/expected"
cmp "$tmp/expected" "$BUILD_TRACE"
rm -f "$BUILD_TRACE" "$APP_TRACE"
"$bash_path" "$tmp/launcher/run.sh" --help > "$tmp/help"
grep -Fq -- '--diagnose-sdl' "$tmp/help"
[ ! -e "$BUILD_TRACE" ] && [ ! -e "$APP_TRACE" ]
for invalid in -j0 -j-1 --jobs= --jobs=abc; do
    rc=0
    "$bash_path" "$tmp/launcher/run.sh" "$invalid" > /dev/null 2>&1 || rc=$?
    [ "$rc" -eq 2 ] && [ ! -e "$BUILD_TRACE" ] && [ ! -e "$APP_TRACE" ]
done

# The real builder runs in another throwaway tree with fake tooling. Simulate
# a failed install after successful configure/build, including a partial SDK.
mkdir -p "$tmp/project/.cache/urho3d-src/.git" \
    "$tmp/project/.cache/urho3d-sdk/include/Urho3D/Engine" \
    "$tmp/project/.cache/urho3d-sdk/include/Urho3D/ThirdParty/SDL" \
    "$tmp/project/.cache/urho3d-sdk/lib/Urho3D" "$tmp/bin"
cp "$root/build.sh" "$tmp/project/build.sh"
printf 'original headers\n' > "$tmp/project/.cache/urho3d-sdk/include/Urho3D/Engine/Application.h"
printf 'original SDL\n' > "$tmp/project/.cache/urho3d-sdk/include/Urho3D/ThirdParty/SDL/SDL_joystick.h"
printf 'original SDK\n' > "$tmp/project/.cache/urho3d-sdk/lib/Urho3D/libUrho3D.a"
printf '#!%s\n' "$bash_path" > "$tmp/bin/fake-cxx"
cat >> "$tmp/bin/fake-cxx" <<'CXX'
if [ "${1:-}" = -print-multiarch ]; then echo 'test-multiarch'; exit 0; fi
case "${1:-}" in
    */src/MyApp.cpp) printf '%s\n' "$@" > "$GAME_CXX_TRACE"; exit 0 ;;
esac
# Preprocessor/link checks only; consume stdin so pipefail doesn't see SIGPIPE.
cat > /dev/null
CXX
printf '#!%s\n' "$bash_path" > "$tmp/bin/pkg-config"
cat >> "$tmp/bin/pkg-config" <<'PC'
exit 1
PC
# The fake SDK source already exists. A git or make invocation is unexpected
# and must never download anything, even when these tools aren't installed.
for tool in git make; do
    printf '#!%s\nexit 1\n' "$bash_path" > "$tmp/bin/$tool"
done
printf '#!%s\n' "$bash_path" > "$tmp/bin/cmake"
cat >> "$tmp/bin/cmake" <<'CMAKE'
set -euo pipefail
case "${1:-}" in
    --version) echo 'cmake version 3.31.0' ;;
    -S)
        build_dir=''
        while [ "$#" -gt 0 ]; do
            if [ "$1" = -B ]; then build_dir="$2"; break; fi
            shift
        done
        [ -n "$build_dir" ]
        mkdir -p "$build_dir/include/Urho3D/ThirdParty/SDL"
        printf '#define SDL_VIDEO_DRIVER_X11 1\n#define SDL_AUDIO_DRIVER_ALSA 1\n' > "$build_dir/include/Urho3D/ThirdParty/SDL/SDL_config.h"
        ;;
    --build)
        printf '%s\n' "$@" > "$CMAKE_TRACE"
        if [ "${FAIL_BUILD:-0}" = 1 ]; then exit 42; fi
        ;;
    --install)
        mkdir -p "$TEST_SDK"
        echo partial > "$TEST_SDK/incomplete-install-marker"
        # Simulate either an error exit or a misleading successful but
        # incomplete installation; both must restore the previous SDK.
        if [ "${INCOMPLETE_INSTALL:-0}" = 1 ]; then exit 0; fi
        exit 42
        ;;
    *) echo "Unexpected fake cmake invocation: $*" >&2; exit 1 ;;
esac
CMAKE
chmod +x "$tmp/bin/fake-cxx" "$tmp/bin/pkg-config" "$tmp/bin/cmake" "$tmp/bin/git" "$tmp/bin/make"
export PATH="$tmp/bin:$PATH" CXX="$tmp/bin/fake-cxx" CMAKE_TRACE="$tmp/cmake-args" GAME_CXX_TRACE="$tmp/game-cxx-args"
export TEST_SDK="$tmp/project/.cache/urho3d-sdk"
for job_style in compact-short compact-long spaced-short spaced-long; do
    case "$job_style" in
        compact-short) set -- -j4; expected_job=-j4 ;;
        compact-long)  set -- --jobs=5; expected_job=-j5 ;;
        spaced-short)  set -- -j 2; expected_job=-j2 ;;
        spaced-long)   set -- --jobs 3; expected_job=-j3 ;;
    esac
    rc=0
    "$bash_path" "$tmp/project/build.sh" --rebuild-urho3d "$@" > "$tmp/builder-log" 2>&1 || rc=$?
    [ "$rc" -eq 1 ]
    grep -Fxq -- "$expected_job" "$CMAKE_TRACE"
    grep -Fq 'Не удалось установить Urho3D SDK' "$tmp/builder-log"
    [ "$(cat "$TEST_SDK/lib/Urho3D/libUrho3D.a")" = 'original SDK' ]
    [ "$(cat "$TEST_SDK/include/Urho3D/Engine/Application.h")" = 'original headers' ]
    [ ! -e "$TEST_SDK/incomplete-install-marker" ]
    if find "$tmp/project/.cache" -maxdepth 1 -name 'urho3d-sdk-backup.*' | grep -q .; then
        echo 'SDK backup was not removed after rollback' >&2
        exit 1
    fi
done

rc=0
INCOMPLETE_INSTALL=1 "$bash_path" "$tmp/project/build.sh" --rebuild-urho3d -j2 > "$tmp/builder-log" 2>&1 || rc=$?
[ "$rc" -eq 1 ]
grep -Fq 'установка неполная' "$tmp/builder-log"
[ "$(cat "$TEST_SDK/lib/Urho3D/libUrho3D.a")" = 'original SDK' ]
[ ! -e "$TEST_SDK/incomplete-install-marker" ]

rc=0
FAIL_BUILD=1 "$bash_path" "$tmp/project/build.sh" --rebuild-urho3d -j2 > "$tmp/builder-log" 2>&1 || rc=$?
[ "$rc" -ne 0 ]
[ "$(cat "$TEST_SDK/lib/Urho3D/libUrho3D.a")" = 'original SDK' ]
[ ! -e "$TEST_SDK/incomplete-install-marker" ]

# First install failure must not leave a partial cache behind.
mv "$TEST_SDK" "$tmp/saved-sdk"
rc=0
"$bash_path" "$tmp/project/build.sh" -j2 > "$tmp/builder-log" 2>&1 || rc=$?
[ "$rc" -eq 1 ] && [ ! -e "$TEST_SDK" ]
mv "$tmp/saved-sdk" "$TEST_SDK"

rm -f "$CMAKE_TRACE"
for invalid in -j0 -j-1 --jobs= --jobs=abc; do
    rc=0
    "$bash_path" "$tmp/project/build.sh" "$invalid" > /dev/null 2>&1 || rc=$?
    [ "$rc" -eq 2 ] && [ ! -e "$CMAKE_TRACE" ]
done
rc=0
URHO3D_HOME="$tmp/external-sdk" "$bash_path" "$tmp/project/build.sh" --rebuild-urho3d > "$tmp/external-log" 2>&1 || rc=$?
[ "$rc" -eq 1 ] && [ ! -e "$CMAKE_TRACE" ]
grep -Fq 'не внешний SDK' "$tmp/external-log"
[ "$(cat "$TEST_SDK/lib/Urho3D/libUrho3D.a")" = 'original SDK' ]

# A cached .pc from another SDK must not override an explicit SDK or leak its
# compiler/linker flags. Stop after discovery, before any actual game build.
mkdir -p "$tmp/external-sdk/include/Urho3D/Engine" "$tmp/external-sdk/include/Urho3D/ThirdParty/SDL"
printf 'external headers\n' > "$tmp/external-sdk/include/Urho3D/Engine/Application.h"
printf 'external SDL\n' > "$tmp/external-sdk/include/Urho3D/ThirdParty/SDL/SDL_joystick.h"
printf 'external library\n' > "$tmp/external-sdk/lib/Urho3D/libUrho3D.a"
printf '#!%s\n' "$bash_path" > "$tmp/bin/pkg-config"
cat >> "$tmp/bin/pkg-config" <<'PC'
printf '%s\n' "$*" >> "$PKG_TRACE"
[ "${DISABLE_URHO_PC:-0}" = 1 ] && exit 1
case "$*" in
    '--exists Urho3D') exit 0 ;;
    '--variable=includedir Urho3D') echo "$TEST_SDK/include" ;;
    '--variable=libdir Urho3D') echo "$TEST_SDK/lib/Urho3D" ;;
    '--cflags Urho3D') echo '-DWRONG_SDK_PC' ;;
    '--libs Urho3D') echo '-lUrho3D -lwrong_sdk' ;;
    *) exit 1 ;;
esac
PC
chmod +x "$tmp/bin/pkg-config"
export PKG_TRACE="$tmp/pkg-config-args"
for selection in home include; do
    : > "$PKG_TRACE"
    rc=0
    if [ "$selection" = home ]; then
        URHO3D_HOME="$tmp/external-sdk" "$bash_path" "$tmp/project/build.sh" -j2 > "$tmp/selection-log" 2>&1 || rc=$?
    else
        URHO_INC="$tmp/external-sdk/include" "$bash_path" "$tmp/project/build.sh" -j2 > "$tmp/selection-log" 2>&1 || rc=$?
    fi
    [ "$rc" -ne 0 ] # This throwaway project intentionally has no game assets.
    grep -Fq "Заголовки:  $tmp/external-sdk/include" "$tmp/selection-log"
    grep -Fq "Библиотеки: $tmp/external-sdk/lib/Urho3D" "$tmp/selection-log"
    grep -Fq 'Пропускаю Urho3D.pc из другого SDK' "$tmp/selection-log"
    if grep -Eq '^--(cflags|libs) Urho3D$' "$PKG_TRACE"; then
        echo 'Builder read compiler/linker flags from a different SDK.' >&2
        exit 1
    fi
done

# Explicit SDKs using a Debian-style multiarch library directory are valid.
mkdir -p "$tmp/multiarch-sdk/include/Urho3D/Engine" "$tmp/multiarch-sdk/lib/test-multiarch/Urho3D"
printf 'multiarch headers\n' > "$tmp/multiarch-sdk/include/Urho3D/Engine/Application.h"
printf 'multiarch library\n' > "$tmp/multiarch-sdk/lib/test-multiarch/Urho3D/libUrho3D.a"
rc=0
URHO3D_HOME="$tmp/multiarch-sdk" "$bash_path" "$tmp/project/build.sh" -j2 > "$tmp/multiarch-log" 2>&1 || rc=$?
[ "$rc" -ne 0 ] # This throwaway project intentionally has no game assets.
grep -Fq "Заголовки:  $tmp/multiarch-sdk/include" "$tmp/multiarch-log"
grep -Fq "Библиотеки: $tmp/multiarch-sdk/lib/test-multiarch/Urho3D" "$tmp/multiarch-log"

# An explicitly named but incomplete SDK must not silently borrow the other
# half from the local cache (or trigger a download/rebuild).
mkdir -p "$tmp/headers-only/include/Urho3D/Engine" "$tmp/library-only/lib/Urho3D"
printf 'headers only\n' > "$tmp/headers-only/include/Urho3D/Engine/Application.h"
printf 'library only\n' > "$tmp/library-only/lib/Urho3D/libUrho3D.a"
for incomplete in headers-only library-only; do
    rm -f "$CMAKE_TRACE"
    rc=0
    URHO3D_HOME="$tmp/$incomplete" "$bash_path" "$tmp/project/build.sh" -j2 > "$tmp/incomplete-log" 2>&1 || rc=$?
    [ "$rc" -ne 0 ] && [ ! -e "$CMAKE_TRACE" ]
    grep -Fq 'URHO3D_HOME' "$tmp/incomplete-log"
    grep -Fq 'не содержит' "$tmp/incomplete-log"
done

# An explicit include override must not borrow another SDK's library either.
rc=0
URHO_INC="$tmp/headers-only/include" "$bash_path" "$tmp/project/build.sh" -j2 > "$tmp/incomplete-log" 2>&1 || rc=$?
[ "$rc" -ne 0 ] && grep -Fq 'URHO_INC не указывает на полный SDK' "$tmp/incomplete-log"
rc=0
URHO3D_HOME="$tmp/external-sdk" URHO_INC="$tmp/headers-only/include" \
    "$bash_path" "$tmp/project/build.sh" -j2 > "$tmp/incomplete-log" 2>&1 || rc=$?
[ "$rc" -ne 0 ] && grep -Fq 'нельзя смешивать разные SDK' "$tmp/incomplete-log"

# Automatic discovery must pair headers and libraries from one prefix, not a
# headers-only cache with a library-only installation elsewhere.
mkdir -p "$tmp/pair-project/.cache/urho3d-sdk/include/Urho3D/Engine" \
    "$HOME/urho3d-local/lib/Urho3D" "$HOME/urho3d-build/out/include/Urho3D/Engine" \
    "$HOME/urho3d-build/out/lib/Urho3D"
cp "$root/build.sh" "$tmp/pair-project/build.sh"
printf 'partial headers\n' > "$tmp/pair-project/.cache/urho3d-sdk/include/Urho3D/Engine/Application.h"
printf 'other partial library\n' > "$HOME/urho3d-local/lib/Urho3D/libUrho3D.a"
printf 'complete headers\n' > "$HOME/urho3d-build/out/include/Urho3D/Engine/Application.h"
printf 'complete library\n' > "$HOME/urho3d-build/out/lib/Urho3D/libUrho3D.a"
rc=0
DISABLE_URHO_PC=1 "$bash_path" "$tmp/pair-project/build.sh" -j2 > "$tmp/pair-log" 2>&1 || rc=$?
[ "$rc" -ne 0 ] # No game assets in the fixture.
grep -Fq "Заголовки:  $HOME/urho3d-build/out/include" "$tmp/pair-log"
grep -Fq "Библиотеки: $HOME/urho3d-build/out/lib/Urho3D" "$tmp/pair-log"

if [ "$(uname -s)" = Linux ] && command -v nm >/dev/null 2>&1; then
    # A known headless bundled SDL is skipped for auto-discovery; an explicitly
    # selected one fails safely unless the user opts in to headless builds.
    export REAL_NM="$(command -v nm)"
    printf '#!%s\n' "$bash_path" > "$tmp/bin/nm"
    cat >> "$tmp/bin/nm" <<'NM'
if [ "${!#}" = "${MOCK_HEADLESS_ARCHIVE:-}" ]; then
    printf '00000000 T SDL_Init\n'
    for ((i=0; i<4000; ++i)); do printf '00000000 T filler_%s\n' "$i"; done
else
    exec "$REAL_NM" "$@"
fi
NM
    chmod +x "$tmp/bin/nm"
    mkdir -p "$tmp/pair-project/.cache/urho3d-sdk/lib/Urho3D"
    headless_archive="$tmp/pair-project/.cache/urho3d-sdk/lib/Urho3D/libUrho3D.a"
    printf 'headless library\n' > "$headless_archive"
    rc=0
    MOCK_HEADLESS_ARCHIVE="$headless_archive" DISABLE_URHO_PC=1 \
        "$bash_path" "$tmp/pair-project/build.sh" -j2 > "$tmp/pair-log" 2>&1 || rc=$?
    [ "$rc" -ne 0 ]
    grep -Fq 'SDL без X11/Wayland/KMSDRM; пропускаю' "$tmp/pair-log"
    grep -Fq "Библиотеки: $HOME/urho3d-build/out/lib/Urho3D" "$tmp/pair-log"
    rm -f "$CMAKE_TRACE"
    rc=0
    MOCK_HEADLESS_ARCHIVE="$headless_archive" URHO3D_HOME="$tmp/pair-project/.cache/urho3d-sdk" \
        "$bash_path" "$tmp/pair-project/build.sh" -j2 > "$tmp/headless-log" 2>&1 || rc=$?
    [ "$rc" -ne 0 ] && [ ! -e "$CMAKE_TRACE" ]
    grep -Fq 'выбранный SDK содержит SDL без видеодрайвера' "$tmp/headless-log"
    rc=0
    MOCK_HEADLESS_ARCHIVE="$headless_archive" URHO3D_ALLOW_HEADLESS=1 URHO3D_HOME="$tmp/pair-project/.cache/urho3d-sdk" \
        "$bash_path" "$tmp/pair-project/build.sh" -j2 > "$tmp/headless-log" 2>&1 || rc=$?
    [ "$rc" -ne 0 ] # No game assets in the fixture.
    grep -Fq "Библиотеки: $(dirname "$headless_archive")" "$tmp/headless-log"
fi

# Capture the game compiler's arguments for an SDK path containing spaces.
# Fake Python marks the already-stamped texture set valid; no real assets or
# compiler are needed and no files outside the throwaway project are touched.
spaced_sdk="$tmp/SDK with spaces"
mkdir -p "$spaced_sdk/include/Urho3D/Engine" "$spaced_sdk/include/Urho3D/ThirdParty/SDL" \
    "$spaced_sdk/include/Urho3D/ThirdParty/Bullet/LinearMath" "$spaced_sdk/lib/Urho3D" \
    "$spaced_sdk/Source/ThirdParty/SDL/include/generated" "$tmp/project/build/Data/Textures"
printf 'headers\n' > "$spaced_sdk/include/Urho3D/Engine/Application.h"
printf 'SDL\n' > "$spaced_sdk/include/Urho3D/ThirdParty/SDL/SDL_joystick.h"
printf 'generated config\n' > "$spaced_sdk/Source/ThirdParty/SDL/include/generated/SDL_config.h"
printf 'Bullet\n' > "$spaced_sdk/include/Urho3D/ThirdParty/Bullet/LinearMath/btIDebugDraw.h"
printf 'library\n' > "$spaced_sdk/lib/Urho3D/libUrho3D.a"
printf '1024\n' > "$tmp/project/build/Data/Textures/.4k_pbr_stamp"
printf '#!%s\n' "$bash_path" > "$tmp/bin/python3"
cat >> "$tmp/bin/python3" <<'PYTHON'
if [ "${1:-}" = - ]; then cat > /dev/null; fi
exit 0
PYTHON
chmod +x "$tmp/bin/python3"
URHO3D_HOME="$spaced_sdk" "$bash_path" "$tmp/project/build.sh" -j2 > "$tmp/spaced-log" 2>&1
# One occurrence is the normal SDK include; the second is the SDL include.
[ "$(grep -Fxc -- "$spaced_sdk/include/Urho3D/ThirdParty" "$GAME_CXX_TRACE")" -eq 2 ]
grep -Fxq -- "$spaced_sdk/include/Urho3D/ThirdParty/Bullet" "$GAME_CXX_TRACE"
grep -Fxq -- "$spaced_sdk/Source/ThirdParty/SDL/include/generated" "$GAME_CXX_TRACE"
grep -Fxq -- "$spaced_sdk/include/Urho3D/ThirdParty/SDL" "$GAME_CXX_TRACE"
[ "$(cat "$tmp/project/build/.urho_sdk_home")" = "$spaced_sdk" ]
[ "$(cat "$tmp/project/build/.urho_libdir")" = "$spaced_sdk/lib/Urho3D" ]

echo 'Launcher/build flags, diagnostics, SDK pairing, headless detection, and rollback passed.'
