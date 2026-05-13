#!/usr/bin/env bash
#
# ClipToZero installer for macOS.
#
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/stephengeller/ableton-projects/main/plugins/ClipToZero/install.sh | sh
#
# What it does:
#   1. Downloads the latest ClipToZero macOS zip from GitHub Releases.
#   2. Installs VST3 + AU bundles into ~/Library/Audio/Plug-Ins/{VST3,Components}/
#   3. Removes the macOS quarantine attribute so DAWs will actually load them.
#
# ClipToZero is not signed with an Apple Developer certificate; this script
# bypasses Gatekeeper by stripping the com.apple.quarantine xattr that macOS
# applies to anything downloaded from the internet. Trust model is identical
# to manually downloading the zip from the same repo.
#
# Pin a specific version with:
#   CLIPTOZERO_VERSION=v0.4.0 sh -c "$(curl -fsSL https://raw.githubusercontent.com/stephengeller/ableton-projects/main/plugins/ClipToZero/install.sh)"

set -eu

REPO="stephengeller/ableton-projects"
PLUGIN_NAME="ClipToZero"

VST3_DEST="${HOME}/Library/Audio/Plug-Ins/VST3"
AU_DEST="${HOME}/Library/Audio/Plug-Ins/Components"

bold() { printf '\033[1m%s\033[0m\n' "$*"; }
info() { printf '  %s\n' "$*"; }
warn() { printf '\033[33m  ! %s\033[0m\n' "$*"; }
err()  { printf '\033[31m  x %s\033[0m\n' "$*" >&2; }

[ "$(uname -s)" = "Darwin" ] || { err "This installer only runs on macOS. For Windows / Linux, download from https://github.com/${REPO}/releases/latest"; exit 1; }
command -v curl  >/dev/null || { err "curl is required."; exit 1; }
command -v ditto >/dev/null || { err "ditto is required (macOS built-in)."; exit 1; }

bold "${PLUGIN_NAME} installer"

# ---- Resolve the release to install ---------------------------------------
if [ -n "${CLIPTOZERO_VERSION:-}" ]; then
  TAG="$CLIPTOZERO_VERSION"
  info "Installing pinned version ${TAG}"
else
  info "Looking up the latest release..."
  # Minimal JSON parse via sed -- avoids a jq dependency. The 'tag_name'
  # field is always present on a GitHub release JSON, and head -n1 guards
  # against unexpected duplicates in the response.
  API_JSON=$(curl -fsSL "https://api.github.com/repos/${REPO}/releases/latest")
  TAG=$(printf '%s' "$API_JSON" | sed -n 's/.*"tag_name": *"\([^"]*\)".*/\1/p' | head -n1)
  [ -n "$TAG" ] || { err "Could not determine latest version. Network or rate-limit issue?"; exit 1; }
  info "Latest is ${TAG}"
fi

ZIP_NAME="${PLUGIN_NAME}-${TAG}-mac.zip"
ZIP_URL="https://github.com/${REPO}/releases/download/${TAG}/${ZIP_NAME}"
PKG_NAME="${PLUGIN_NAME}-${TAG}-mac"

# ---- Download -------------------------------------------------------------
TMP=$(mktemp -d -t cliptozero-install)
trap 'rm -rf "$TMP"' EXIT INT TERM

info "Downloading ${ZIP_NAME} (~4 MB)..."
curl -fL --progress-bar -o "${TMP}/${ZIP_NAME}" "$ZIP_URL" \
  || { err "Download failed from ${ZIP_URL}"; exit 1; }

# ---- Extract --------------------------------------------------------------
info "Extracting..."
# ditto -x -k preserves macOS resource forks + xattrs + ad-hoc codesign
# inside the .vst3 / .component bundles. unzip would strip them, breaking
# the codesign and prompting Gatekeeper to re-quarantine.
( cd "$TMP" && ditto -x -k "${ZIP_NAME}" . )
[ -d "${TMP}/${PKG_NAME}" ] || { err "Unexpected zip layout - missing ${PKG_NAME}/ directory."; exit 1; }

VST3_SRC="${TMP}/${PKG_NAME}/VST3/${PLUGIN_NAME}.vst3"
AU_SRC="${TMP}/${PKG_NAME}/AU/${PLUGIN_NAME}.component"

[ -d "$VST3_SRC" ] || { err "VST3 bundle missing from zip."; exit 1; }
[ -d "$AU_SRC" ]   || { err "AU bundle missing from zip."; exit 1; }

# ---- Install bundles ------------------------------------------------------
mkdir -p "$VST3_DEST" "$AU_DEST"

install_bundle() {
  local src="$1"
  local dest="$2"
  if [ -d "$dest" ]; then
    info "Replacing existing $(basename "$dest")..."
    rm -rf "$dest" 2>/dev/null || {
      warn "Could not remove ${dest}. Re-running with sudo..."
      sudo rm -rf "$dest"
    }
  fi
  ditto "$src" "$dest" 2>/dev/null || {
    warn "Copy failed. Retrying with sudo..."
    sudo ditto "$src" "$dest"
  }
}

info "Installing VST3 -> ${VST3_DEST}/"
install_bundle "$VST3_SRC" "${VST3_DEST}/${PLUGIN_NAME}.vst3"

info "Installing AU   -> ${AU_DEST}/"
install_bundle "$AU_SRC"   "${AU_DEST}/${PLUGIN_NAME}.component"

# ---- Strip quarantine -----------------------------------------------------
info "Removing macOS quarantine flag..."
xattr -dr com.apple.quarantine "${VST3_DEST}/${PLUGIN_NAME}.vst3"      2>/dev/null || true
xattr -dr com.apple.quarantine "${AU_DEST}/${PLUGIN_NAME}.component"   2>/dev/null || true

# ---- Done -----------------------------------------------------------------
echo
bold "  Installed ${PLUGIN_NAME} ${TAG}"
echo
info "Next step: rescan plugins in your DAW."
info "  Ableton Live -> Preferences > Plug-Ins > Rescan Plug-Ins (hold Option for forced)"
info "  Logic Pro    -> restart Logic (AUs auto-rescan on launch)"
info "  Reaper       -> Preferences > Plug-Ins > VST > Re-scan"
info "  Bitwig       -> Settings > Locations > Plug-ins > Rescan"
echo
info "Find it under manufacturer 'stephengeller' in your plugin browser."
