#!/usr/bin/env bash
# Assembla il pacchetto .deb per Debian/Ubuntu da un build-juce completato.
#
# Uso:
#   packaging/linux/build-deb.sh [VERSIONE]
#
# Richiede: dpkg-deb, fakeroot, e `cmake --build build-juce` gia' eseguito.
#
# I tre formati vanno dove i rispettivi host li cercano di default:
#   VST3       -> /usr/lib/vst3/
#   LV2        -> /usr/lib/lv2/
#   Standalone -> /usr/lib/<pkg>/ con un collegamento in /usr/bin
set -euo pipefail

VERSION="${1:-0.2.0}"
PKG="metal-nam-gear-player"
ARCH="amd64"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="$ROOT/build-juce"
ART="$BUILD/juce/NAMCustom_artefacts/Release"

DIST="$ROOT/dist"
STAGE="$DIST/deb/${PKG}_${VERSION}_${ARCH}"
DEB_OUT="$DIST/${PKG}_${VERSION}_${ARCH}.deb"

STANDALONE="$ART/Standalone/NAM Custom"
VST3="$ART/VST3/NAM Custom.vst3"
LV2="$ART/LV2/NAM Custom.lv2"

# --- controlli ---
for f in "$STANDALONE" "$VST3" "$LV2"; do
    [[ -e "$f" ]] || { echo "Artefatto mancante: $f" >&2; exit 1; }
done
command -v dpkg-deb >/dev/null || { echo "dpkg-deb non installato" >&2; exit 1; }
command -v fakeroot >/dev/null || { echo "fakeroot non installato" >&2; exit 1; }

# --- staging pulito ---
rm -rf "$DIST/deb"
mkdir -p "$STAGE/DEBIAN" \
         "$STAGE/usr/bin" \
         "$STAGE/usr/lib/$PKG" \
         "$STAGE/usr/lib/vst3" \
         "$STAGE/usr/lib/lv2" \
         "$STAGE/usr/share/applications" \
         "$STAGE/usr/share/doc/$PKG"

# --- binari ---
cp    "$STANDALONE" "$STAGE/usr/lib/$PKG/metal-nam-gear-player"
chmod 755           "$STAGE/usr/lib/$PKG/metal-nam-gear-player"
ln -sf "/usr/lib/$PKG/metal-nam-gear-player" "$STAGE/usr/bin/metal-nam-gear-player"
cp -r "$VST3" "$STAGE/usr/lib/vst3/"
cp -r "$LV2"  "$STAGE/usr/lib/lv2/"

# --- documentazione ---
cp "$ROOT/LICENSE"                       "$STAGE/usr/share/doc/$PKG/copyright"
[[ -f "$ROOT/juce/fonts/LICENSE-fonts.txt" ]] && \
    cp "$ROOT/juce/fonts/LICENSE-fonts.txt" "$STAGE/usr/share/doc/$PKG/"
for d in guida-rapida.pdf guida-tecnica.pdf; do
    [[ -f "$ROOT/docs/$d" ]] && cp "$ROOT/docs/$d" "$STAGE/usr/share/doc/$PKG/"
done

# --- voce di menu ---
cat > "$STAGE/usr/share/applications/${PKG}.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Metal NAM Gear Players
GenericName=Amp modeller
Comment=Neural Amp Modeler player con doppio IR, equalizzatore e metronomo
Exec=metal-nam-gear-player
Terminal=false
Categories=AudioVideo;Audio;
Keywords=guitar;amp;NAM;IR;
EOF

# --- metadati del pacchetto ---
INSTALLED_KB="$(du -sk "$STAGE" | cut -f1)"
cat > "$STAGE/DEBIAN/control" <<EOF
Package: $PKG
Version: $VERSION
Section: sound
Priority: optional
Architecture: $ARCH
Maintainer: FabioNET <19152770+fabionet@users.noreply.github.com>
Installed-Size: $INSTALLED_KB
Depends: libc6 (>= 2.31), libstdc++6 (>= 10), libfreetype6, libx11-6, libxext6, libxrandr2, libxcursor1, libxinerama1, libasound2 | libasound2t64, libcurl4 | libcurl4t64
Description: Metal NAM Gear Players - Neo Edition
 Player per modelli Neural Amp Modeler con catena completa: gate, compressore,
 overdrive e distorsione, due caricatori di risposte all'impulso con
 bilanciamento, equalizzatore a cinque bande con analizzatore di spettro,
 sezione di potenza, effetti e metronomo.
 .
 Il pacchetto installa il programma autonomo, il plugin VST3 e il plugin LV2.
EOF

# --- costruzione ---
fakeroot dpkg-deb --build --root-owner-group "$STAGE" "$DEB_OUT" >/dev/null
echo "Creato: $DEB_OUT"
dpkg-deb --info "$DEB_OUT" | sed -n '1,12p'
