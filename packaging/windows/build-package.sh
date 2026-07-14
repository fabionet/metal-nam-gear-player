#!/usr/bin/env bash
# Assemble Windows distribution packages (portable zip + NSIS installer)
# from a completed MinGW build tree.
#
# Usage:
#   packaging/windows/build-package.sh [VERSION]
#
# Requires: zip, makensis (nsis), a completed `cmake --build build-win`.
# Docs source: sibling checkout of neural-amp-modeler-lv2 (juce-rewrite branch).
set -euo pipefail

VERSION="${1:-0.1.1}"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="$ROOT/build-win"
DOCS_SRC="${DOCS_SRC:-$ROOT/../neural-amp-modeler-lv2/docs}"

DIST="$ROOT/dist"
STAGE="$DIST/staging"
BASENAME="MetalNAMGearPlayer-windows-x64"
ZIP_OUT="$DIST/${BASENAME}-portable.zip"
NSI_OUT="$DIST/${BASENAME}-setup.exe"

STANDALONE="$BUILD/juce/NAMCustom_artefacts/Release/Standalone/NAM Custom.exe"
VST3_BUNDLE="$BUILD/juce/NAMCustom_artefacts/Release/VST3/NAM Custom.vst3"

# --- checks ---
for f in "$STANDALONE" "$VST3_BUNDLE"; do
    [[ -e "$f" ]] || { echo "Missing artefact: $f" >&2; exit 1; }
done
for d in guida-rapida.pdf guida-tecnica.pdf; do
    [[ -f "$DOCS_SRC/$d" ]] || { echo "Missing doc: $DOCS_SRC/$d" >&2; exit 1; }
done
command -v zip      >/dev/null || { echo "zip not installed" >&2; exit 1; }
command -v makensis >/dev/null || { echo "makensis not installed (apt install nsis)" >&2; exit 1; }

# --- clean staging ---
rm -rf "$DIST"
mkdir -p "$STAGE/docs"

# --- populate ---
cp "$STANDALONE"            "$STAGE/"
cp -r "$VST3_BUNDLE"        "$STAGE/"
cp "$ROOT/LICENSE"          "$STAGE/"
cp "$DOCS_SRC/guida-rapida.pdf"  "$STAGE/docs/"
cp "$DOCS_SRC/guida-tecnica.pdf" "$STAGE/docs/"

cat > "$STAGE/README.txt" <<EOF
METAL NAM GEAR PLAYER v${VERSION} - Windows x64 portable

Contents
--------
NAM Custom.exe        Standalone application (ASIO / MME / DirectSound)
NAM Custom.vst3/      VST3 plugin bundle
docs/                 Italian user guides (Guida Rapida + Guida Tecnica)
LICENSE               AGPL-3.0-or-later

Installation
------------
1. Copy the whole "NAM Custom.vst3" folder to your VST3 directory:
     System-wide:  C:\\Program Files\\Common Files\\VST3\\
     Per-user:     %APPDATA%\\VST3\\
   Then rescan the folder in your DAW.

2. Double-click "NAM Custom.exe" for standalone use.

3. Load a .nam model (from https://tone3000.com) and optionally an IR .wav
   through the plugin UI. See docs\\guida-rapida.pdf for a quick start.

Support
-------
https://github.com/fabionet/metal-nam-gear-player
EOF

# --- portable zip ---
(
    cd "$STAGE/.."
    mv staging "$BASENAME"
    zip -r -q "$ZIP_OUT" "$BASENAME"
    mv "$BASENAME" staging
)
echo "  portable zip -> $(du -h "$ZIP_OUT" | cut -f1)  $ZIP_OUT"

# --- NSIS installer ---
makensis -V2 \
    -DVERSION="$VERSION" \
    -DSTAGING="$STAGE" \
    -DOUTFILE="$NSI_OUT" \
    "$ROOT/packaging/windows/installer.nsi"
echo "  installer    -> $(du -h "$NSI_OUT" | cut -f1)  $NSI_OUT"

echo "Done."
