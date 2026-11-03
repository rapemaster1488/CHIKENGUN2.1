#!/usr/bin/env bash
# ============================================================================
# Упаковщик портативного релиза CHIKENGUN 2.4 (C++17 / Urho3D, Linux x86_64)
# Собирает готовый к скачиванию автономный архив:
#   dist/CHIKENGUN-2.4-Linux-x86_64.tar.gz + dist/SHA256SUMS.txt
# Внутри архива: нативный бинарник MyShooter, libUrho3D (в ./lib с $ORIGIN/lib),
# очищенные Data/ и CoreData/ без мусорных дублей, settings.ini и лаунчер.
# ============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="${VERSION:-2.4}"
ARCH="$(uname -m)"
PKG_NAME="CHIKENGUN-${VERSION}-Linux-${ARCH}"
STAGE_DIR="$ROOT/build/stage/${PKG_NAME}"
OUT_DIR="${OUT_DIR:-$ROOT/build/release}"

if [ "${1:-}" = "--help" ] || [ "${1:-}" = "-h" ]; then
    echo "Использование: [TEX_SIZE=4096] [URHO3D_HOME=/путь] ./tools/package_linux.sh"
    exit 0
fi

# 1. Сборка C++ клиента и генерация PBR текстур (если ещё не собран)
if [ ! -x "$ROOT/build/MyShooter" ] || [ "${FORCE_REBUILD:-0}" = "1" ]; then
    "$ROOT/build.sh"
fi

echo "[pack] Подготовка чистой директории релиза: $STAGE_DIR"
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR/lib" "$STAGE_DIR/Screenshots" "$OUT_DIR"

# 2. Бинарник и динамические библиотеки движка (если Urho3D собран как .so)
cp -f "$ROOT/build/MyShooter" "$STAGE_DIR/MyShooter"
chmod +x "$STAGE_DIR/MyShooter"
if command -v strip >/dev/null 2>&1; then
    strip --strip-unneeded "$STAGE_DIR/MyShooter" 2>/dev/null || true
fi

for ld in \
    "${URHO3D_HOME:-}/lib" \
    "${URHO3D_HOME:-}/build/lib" \
    "$HOME/urho3d/build/lib" \
    "$HOME/urho3d_build/lib" \
    "/usr/local/lib"
do
    [ -d "$ld" ] || continue
    if ls "$ld"/libUrho3D.so* >/dev/null 2>&1; then
        cp -Lf "$ld"/libUrho3D.so* "$STAGE_DIR/lib/" 2>/dev/null || true
        break
    fi
done

# 3. Копируем только нужные игровые ресурсы (без дублей Data/Data и примеров Urho2D)
cp -rf "$ROOT/build/CoreData" "$STAGE_DIR/CoreData"
mkdir -p "$STAGE_DIR/Data"
for sub in Fonts Materials Models Music Objects Particle PostProcess Sounds Textures UI; do
    if [ -d "$ROOT/build/Data/$sub" ]; then
        cp -rf "$ROOT/build/Data/$sub" "$STAGE_DIR/Data/$sub"
    fi
done
rm -rf "$STAGE_DIR/Data/Data" 2>/dev/null || true

cp -f "$ROOT/bin/settings.ini" "$STAGE_DIR/settings.ini"
cp -f "$ROOT/README.md" "$STAGE_DIR/README.md"
cp -f "$ROOT/LICENSE" "$STAGE_DIR/LICENSE"

# 4. Автономный скрипт запуска (не требует установленного Urho3D в системе)
cat > "$STAGE_DIR/ChikenGun.sh" <<'LAUNCH'
#!/usr/bin/env bash
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$HERE/lib:${LD_LIBRARY_PATH:-}"
cd "$HERE"
exec "$HERE/MyShooter" "$@"
LAUNCH
chmod +x "$STAGE_DIR/ChikenGun.sh"

# 5. Desktop-ярлык для Linux
cat > "$STAGE_DIR/ChikenGun.desktop" <<'DESK'
[Desktop Entry]
Type=Application
Name=CHIKENGUN 2.4 (4K Ultra C++)
Comment=Нативный 3D кибер-шутер на C++17 / Urho3D
Exec=./ChikenGun.sh
Terminal=false
Categories=Game;ActionGame;
DESK

# 6. Упаковка архива и контрольная сумма SHA-256
TARBALL="$OUT_DIR/${PKG_NAME}.tar.gz"
tar -C "$ROOT/build/stage" -czf "$TARBALL" "$PKG_NAME"
(cd "$OUT_DIR" && sha256sum "${PKG_NAME}.tar.gz" > SHA256SUMS.txt)

echo "================================================================"
echo "[pack] Готов скачиваемый C++ релиз:"
ls -lh "$TARBALL" "$OUT_DIR/SHA256SUMS.txt"
cat "$OUT_DIR/SHA256SUMS.txt"
echo "================================================================"
