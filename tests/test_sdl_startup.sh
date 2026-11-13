#!/usr/bin/env bash
# Exercise the actual SDL linked into the game, without requiring a GUI or audio device.
set -euo pipefail
exe="$1"
# CMake's build tree keeps the portable install RPATH. For a selected shared
# SDK, supply its library directory for this test while retaining caller paths.
if [ -n "${2:-}" ]; then
    export LD_LIBRARY_PATH="$2${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

require_line() {
    if ! grep -Fq -- "$1" "$2"; then
        printf 'Expected SDL diagnostic line not found: %s\n' "$1" >&2
        cat "$2" >&2
        exit 1
    fi
}

# A bogus explicit backend must fail promptly, show the compiled drivers and
# still diagnose audio. A separate process ensures the fallback cannot leak.
if env -u DISPLAY -u WAYLAND_DISPLAY -u XDG_RUNTIME_DIR \
    SDL_VIDEODRIVER=chikengun-no-such-video SDL_AUDIODRIVER=chikengun-no-such-audio \
    "$exe" --diagnose-sdl >"$tmp/diagnose" 2>&1; then
    echo "SDL diagnostic unexpectedly accepted a nonexistent video driver" >&2
    exit 1
fi
require_line 'Video drivers (built):' "$tmp/diagnose"
require_line 'Audio drivers (built):' "$tmp/diagnose"
require_line 'Video initialization failed:' "$tmp/diagnose"
require_line 'DISPLAY and WAYLAND_DISPLAY are both unset' "$tmp/diagnose"
require_line 'SDL_VIDEODRIVER is explicitly set' "$tmp/diagnose"
require_line 'Audio initialization failed:' "$tmp/diagnose"
require_line 'Sound is unavailable or disabled' "$tmp/diagnose"

# The ordinary launch also fails cleanly rather than starting a fake display.
if env -u DISPLAY -u WAYLAND_DISPLAY -u XDG_RUNTIME_DIR \
    SDL_VIDEODRIVER=chikengun-no-such-video SDL_AUDIODRIVER=chikengun-no-such-audio \
    "$exe" >"$tmp/launch" 2>&1; then
    echo "Game unexpectedly started without a video backend" >&2
    exit 1
fi
require_line 'Video initialization failed:' "$tmp/launch"

# The SDL disk driver opens its output with wb. A diagnostic must never
# truncate a user's existing recording, even when the display is unavailable.
if grep -Eq 'Audio drivers \(built\):.*(^|[[:space:]])disk([[:space:]]|$)' "$tmp/diagnose"; then
    printf 'existing recording\n' > "$tmp/disk-audio.raw"
    if env -u DISPLAY -u WAYLAND_DISPLAY -u XDG_RUNTIME_DIR \
        SDL_VIDEODRIVER=chikengun-no-such-video SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE="$tmp/disk-audio.raw" \
        "$exe" --diagnose-sdl >"$tmp/disk" 2>&1; then
        echo "Disk-audio diagnostic unexpectedly accepted a nonexistent video driver" >&2
        exit 1
    fi
    require_line 'output probe skipped to avoid overwriting its file' "$tmp/disk"
    [ "$(cat "$tmp/disk-audio.raw")" = 'existing recording' ] || {
        echo "SDL diagnostic unexpectedly changed the disk-audio output" >&2
        exit 1
    }
fi

if grep -Eq 'Video drivers \(built\):.*(^|[[:space:]])dummy([[:space:]]|$)' "$tmp/diagnose"; then
    if env -u DISPLAY -u WAYLAND_DISPLAY -u XDG_RUNTIME_DIR \
        SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=chikengun-no-such-audio \
        "$exe" --diagnose-sdl >"$tmp/dummy-video" 2>&1; then
        echo "Dummy video must not count as a usable desktop" >&2
        exit 1
    fi
    require_line 'dummy video driver cannot display the game' "$tmp/dummy-video"
fi

if grep -Eq 'Audio drivers \(built\):.*(^|[[:space:]])dummy([[:space:]]|$)' "$tmp/diagnose"; then
    if env -u DISPLAY -u WAYLAND_DISPLAY -u XDG_RUNTIME_DIR \
        SDL_VIDEODRIVER=chikengun-no-such-video SDL_AUDIODRIVER=chikengun-no-such-audio \
        "$exe" --diagnose-sdl -nosound >"$tmp/nosound" 2>&1; then
        echo "No-sound diagnostic unexpectedly accepted a nonexistent video driver" >&2
        exit 1
    fi
    require_line 'Audio disabled by -nosound' "$tmp/nosound"
    if grep -Fq 'Audio initialization failed:' "$tmp/nosound"; then
        echo "-nosound should use the built-in silent audio backend" >&2
        exit 1
    fi
fi

echo "SDL startup diagnostics: no display, invalid backends, silent audio and dummy-video rejection passed."
