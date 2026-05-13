#!/usr/bin/env bash
# Package the current macOS build of ClipToZero into a shareable zip.
#
# Usage:
#   ./dist/package-mac.sh                # rebuild + package
#   ./dist/package-mac.sh --skip-build   # package what's already built
#   ./dist/package-mac.sh --release      # also create a GitHub release via gh
#
# Output: dist/output/ClipToZero-vX.Y.Z-mac.zip
set -euo pipefail

# Resolve the plugin root (parent of this script's directory).
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PLUGIN_ROOT"

SKIP_BUILD=0
DO_RELEASE=0
for arg in "$@"; do
    case "$arg" in
        --skip-build) SKIP_BUILD=1 ;;
        --release)    DO_RELEASE=1 ;;
        -h|--help)
            head -n 8 "${BASH_SOURCE[0]}" | tail -n 7
            exit 0
            ;;
        *) echo "Unknown arg: $arg"; exit 2 ;;
    esac
done

# ---- Resolve version from CMakeLists -----------------------------------
VERSION="$(grep -E '^project\(.* VERSION ' CMakeLists.txt | sed -E 's/.*VERSION ([0-9.]+).*/\1/' | head -n1)"
if [[ -z "$VERSION" ]]; then
    VERSION="0.0.0-dev"
fi
GIT_SHA="$(git -C "$PLUGIN_ROOT" rev-parse --short HEAD 2>/dev/null || echo "nogit")"

PKG_NAME="ClipToZero-v${VERSION}-mac"
OUT_DIR="$PLUGIN_ROOT/dist/output"
PKG_DIR="$OUT_DIR/$PKG_NAME"
ZIP_PATH="$OUT_DIR/${PKG_NAME}.zip"

echo "==> ClipToZero packaging"
echo "    version: ${VERSION} (git ${GIT_SHA})"
echo "    output:  $ZIP_PATH"

# ---- Rebuild (unless skipped) ------------------------------------------
if [[ "$SKIP_BUILD" -eq 0 ]]; then
    echo "==> Building Release (cmake --build)"
    cmake --build build --config Release > /tmp/cliptozero-build.log 2>&1 || {
        echo "Build failed. Last 40 lines:"
        tail -n 40 /tmp/cliptozero-build.log
        exit 1
    }
    echo "    done."
fi

# ---- Verify expected artefacts exist -----------------------------------
ART_DIR="$PLUGIN_ROOT/build/ClipToZero_artefacts/Release"
VST3_SRC="$ART_DIR/VST3/ClipToZero.vst3"
AU_SRC="$ART_DIR/AU/ClipToZero.component"
APP_SRC="$ART_DIR/Standalone/ClipToZero.app"

for path in "$VST3_SRC" "$AU_SRC" "$APP_SRC"; do
    if [[ ! -d "$path" ]]; then
        echo "ERROR: expected build artefact missing: $path"
        echo "Run a fresh build (no --skip-build) or fix CMakeLists output paths."
        exit 1
    fi
done

# ---- Stage the package directory ---------------------------------------
echo "==> Staging $PKG_DIR"
rm -rf "$PKG_DIR" "$ZIP_PATH"
mkdir -p "$PKG_DIR/VST3" "$PKG_DIR/AU" "$PKG_DIR/Standalone"

# Use ditto so macOS bundle metadata + ad-hoc code signature are preserved.
ditto "$VST3_SRC" "$PKG_DIR/VST3/ClipToZero.vst3"
ditto "$AU_SRC"   "$PKG_DIR/AU/ClipToZero.component"
ditto "$APP_SRC"  "$PKG_DIR/Standalone/ClipToZero.app"

# ---- Optional: Developer ID code-signing + notarisation -----------------
# Behaviour is env-var gated so this script remains a no-op for free /
# open-source builds (the default).
#
# To produce a properly signed + notarised build (post Apple-Developer-ID
# purchase), set:
#   APPLE_DEVELOPER_ID="Developer ID Application: Your Name (TEAMID12345)"
#   APPLE_NOTARY_PROFILE="cliptozero-notary"   # name of `notarytool` keychain profile
#
# The keychain profile is created once via:
#   xcrun notarytool store-credentials cliptozero-notary \
#       --apple-id YOU@example.com --team-id TEAMID12345 \
#       --password APP-SPECIFIC-PASSWORD
#
# Without both env vars, this block prints a notice and skips. The bundles
# still get JUCE's ad-hoc signature (works locally, not over the internet).
if [[ -n "${APPLE_DEVELOPER_ID:-}" ]]; then
    echo "==> Codesigning bundles with Developer ID"
    for bundle in \
        "$PKG_DIR/VST3/ClipToZero.vst3" \
        "$PKG_DIR/AU/ClipToZero.component" \
        "$PKG_DIR/Standalone/ClipToZero.app"
    do
        # --options runtime is required for notarisation (enables Hardened
        # Runtime). --deep walks the bundle so nested frameworks/dylibs
        # get re-signed too. --timestamp embeds a secure timestamp from
        # Apple's TSA -- also required by notarisation.
        codesign --force --deep --options runtime --timestamp \
                 --sign "$APPLE_DEVELOPER_ID" "$bundle"
        codesign --verify --strict --verbose=2 "$bundle"
    done

    if [[ -n "${APPLE_NOTARY_PROFILE:-}" ]]; then
        echo "==> Notarising (this takes a few minutes)"
        # notarytool requires a zip or DMG; submit a tmp zip containing
        # the three bundles, then staple to each bundle on success.
        NOTARY_ZIP="$OUT_DIR/.notary-${PKG_NAME}.zip"
        rm -f "$NOTARY_ZIP"
        ( cd "$PKG_DIR" && ditto -c -k --keepParent . "$NOTARY_ZIP" )
        xcrun notarytool submit "$NOTARY_ZIP" \
              --keychain-profile "$APPLE_NOTARY_PROFILE" \
              --wait
        rm -f "$NOTARY_ZIP"

        # Staple the notarisation ticket onto each bundle so Gatekeeper
        # can verify it offline.
        for bundle in \
            "$PKG_DIR/VST3/ClipToZero.vst3" \
            "$PKG_DIR/AU/ClipToZero.component" \
            "$PKG_DIR/Standalone/ClipToZero.app"
        do
            xcrun stapler staple "$bundle"
        done
        echo "    notarised + stapled."
    else
        echo "    NOTE: APPLE_NOTARY_PROFILE not set -- signed but not notarised."
        echo "    Bundles will install but Gatekeeper will warn on first launch."
    fi
else
    echo "    [no APPLE_DEVELOPER_ID env var; shipping with JUCE's ad-hoc signature]"
    echo "    Users will need to run 'xattr -dr com.apple.quarantine' on the bundles."
fi

cp "$SCRIPT_DIR/INSTALL.md" "$PKG_DIR/INSTALL.md"

# Record build provenance (version, git SHA, date) so a future bug report
# can identify exactly which build the user has.
cat > "$PKG_DIR/BUILD_INFO.txt" <<EOF
ClipToZero ${VERSION}
git: ${GIT_SHA}
built: $(date -u +"%Y-%m-%dT%H:%M:%SZ")
host: $(uname -mrsv)
EOF

# ---- Create the zip with ditto -----------------------------------------
echo "==> Archiving $ZIP_PATH"
( cd "$OUT_DIR" && ditto -c -k --keepParent "$PKG_NAME" "$ZIP_PATH" )

SIZE_HUMAN="$(du -h "$ZIP_PATH" | cut -f1)"
echo "==> Done."
echo "    $ZIP_PATH  ($SIZE_HUMAN)"

# ---- Optional: create / update GitHub release --------------------------
if [[ "$DO_RELEASE" -eq 1 ]]; then
    if ! command -v gh > /dev/null 2>&1; then
        echo "WARN: --release given but gh CLI not installed. Skipping."
        exit 0
    fi
    TAG="v${VERSION}"
    echo "==> Publishing to GitHub release ${TAG}"
    if gh release view "$TAG" > /dev/null 2>&1; then
        gh release upload "$TAG" "$ZIP_PATH" --clobber
    else
        gh release create "$TAG" "$ZIP_PATH" \
            --title "ClipToZero ${TAG}" \
            --notes "Local build packaged from commit ${GIT_SHA}. macOS only — Windows/Linux builds require building from source (see BUILD-FROM-SOURCE.md)."
    fi
fi
