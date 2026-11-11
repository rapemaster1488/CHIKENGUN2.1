#!/usr/bin/env bash
# ============================================================================
# Сборщик автономного портативного Linux-архива CHIKENGUN 2.5 «ПЕРО СУДЬБЫ»
# Результат: build/release/CHIKENGUN-2.5-Linux-x86_64.tar.gz (+ SHA256SUMS.txt)
# Можно скачать, распаковать и запустить на любом Linux без установленного Urho3D.
# ============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ARCH="$(uname -m)"
PKG_NAME="CHIKENGUN-2.5-Linux-${ARCH}"
STAGE="$ROOT/build/release/$PKG_NAME"
OUT_TAR="$ROOT/build/release/${PKG_NAME}.tar.gz"

if [ ! -x "$ROOT/build/MyShooter" ]; then
    echo "[package] Бинарник отсутствует — запускаю build.sh..."
    "$ROOT/build.sh" "$@"
fi

rm -rf "$STAGE"
mkdir -p "$STAGE/lib" "$STAGE/Data" "$STAGE/CoreData"

cp "$ROOT/build/MyShooter" "$STAGE/MyShooter"
chmod +x "$STAGE/MyShooter"

# Копируем libUrho3D.so / сторонние несистемные библиотеки, если движок слинкован динамически
if command -v ldd >/dev/null 2>&1; then
    for dep in $(ldd "$STAGE/MyShooter" | awk '/=> \//{print $3}'); do
        base="$(basename "$dep")"
        case "$base" in
            libUrho3D*|libSDL2*|libBullet*)
                cp -L "$dep" "$STAGE/lib/" 2>/dev/null || true
                ;;
        esac
    done
fi

cp -rf "$ROOT/build/CoreData"/. "$STAGE/CoreData"/
cp -rf "$ROOT/build/Data"/. "$STAGE/Data"/
[ -f "$ROOT/build/settings.ini" ] && cp "$ROOT/build/settings.ini" "$STAGE/settings.ini" || cp "$ROOT/bin/settings.ini" "$STAGE/settings.ini"

cat > "$STAGE/ChikenGun.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$HERE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
cd "$HERE"
exec "$HERE/MyShooter" "$@"
EOF
chmod +x "$STAGE/ChikenGun.sh"

cat > "$STAGE/ChikenGun.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=CHIKENGUN 2.5 — Перо Судьбы
Comment=Нативный 3D-шутер на C++17 / Urho3D с баней, казино и Pixel Arcade
Exec=sh -c 'cd "$(dirname "$0")" && ./ChikenGun.sh' %k
Terminal=false
Categories=Game;ActionGame;
EOF

mkdir -p "$ROOT/build/release"
tar -C "$ROOT/build/release" -czf "$OUT_TAR" "$PKG_NAME"
(
    cd "$ROOT/build/release"
    sha256sum "$(basename "$OUT_TAR")" > SHA256SUMS.txt
)

echo "================================================================"
echo "Портативный Linux-релиз собран:"
echo "  $OUT_TAR"
echo "  $ROOT/build/release/SHA256SUMS.txt"
echo "================================================================"
