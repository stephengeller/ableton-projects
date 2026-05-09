#!/usr/bin/env bash
# Package the current Linux build of ClipToZero into a shareable zip.
#
# Usage:
#   ./dist/package-linux.sh                # rebuild + zip
#   ./dist/package-linux.sh --skip-build   # zip whatever is in build/
#
# Output: dist/output/ClipToZero-vX.Y.Z-linux.zip
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PLUGIN_ROOT"

SKIP_BUILD=0
for arg in "$@"; do
    case "$arg" in
        --skip-build) SKIP_BUILD=1 ;;
        -h|--help)    head -n 7 "${BASH_SOURCE[0]}" | tail -n 6; exit 0 ;;
        *) echo "Unknown arg: $arg"; exit 2 ;;
    esac
done

VERSION="$(grep -E '^project\(.* VERSION ' CMakeLists.txt | sed -E 's/.*VERSION ([0-9.]+).*/\1/' | head -n1)"
[[ -z "$VERSION" ]] && VERSION="0.0.0-dev"
GIT_SHA="$(git -C "$PLUGIN_ROOT" rev-parse --short HEAD 2>/dev/null || echo "nogit")"

PKG_NAME="ClipToZero-v${VERSION}-linux"
OUT_DIR="$PLUGIN_ROOT/dist/output"
PKG_DIR="$OUT_DIR/$PKG_NAME"
ZIP_PATH="$OUT_DIR/${PKG_NAME}.zip"

echo "==> ClipToZero packaging (Linux)"
echo "    version: ${VERSION} (git ${GIT_SHA})"
echo "    output:  $ZIP_PATH"

if [[ "$SKIP_BUILD" -eq 0 ]]; then
    echo "==> Building Release"
    cmake --build build --config Release > /tmp/cliptozero-build.log 2>&1 || {
        echo "Build failed. Last 40 lines:"
        tail -n 40 /tmp/cliptozero-build.log
        exit 1
    }
fi

ART_DIR="$PLUGIN_ROOT/build/ClipToZero_artefacts/Release"
VST3_SRC="$ART_DIR/VST3/ClipToZero.vst3"
APP_SRC="$ART_DIR/Standalone/ClipToZero"

[[ -d "$VST3_SRC" ]] || { echo "ERROR: missing $VST3_SRC"; exit 1; }

echo "==> Staging $PKG_DIR"
rm -rf "$PKG_DIR" "$ZIP_PATH"
mkdir -p "$PKG_DIR/VST3"

cp -R "$VST3_SRC" "$PKG_DIR/VST3/"

if [[ -f "$APP_SRC" ]]; then
    mkdir -p "$PKG_DIR/Standalone"
    cp "$APP_SRC" "$PKG_DIR/Standalone/"
fi

cp "$SCRIPT_DIR/INSTALL.md" "$PKG_DIR/INSTALL.md"

cat > "$PKG_DIR/BUILD_INFO.txt" <<EOF
ClipToZero ${VERSION}
git: ${GIT_SHA}
built: $(date -u +"%Y-%m-%dT%H:%M:%SZ")
host: $(uname -mrsv)
EOF

echo "==> Archiving"
( cd "$OUT_DIR" && zip -qr "${PKG_NAME}.zip" "$PKG_NAME" )

SIZE_HUMAN="$(du -h "$ZIP_PATH" | cut -f1)"
echo "==> Done."
echo "    $ZIP_PATH  ($SIZE_HUMAN)"
