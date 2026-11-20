#!/bin/bash
# Builds "SubRoo-Rooms-<version>-macOS.pkg" from the CMake artefacts.
# usage: build_pkg.sh <artefacts/Release dir> <version> <out dir> [pack key] [path to 7zz]
set -euo pipefail
A="$1"; VER="$2"; OUT="$3"; KEY="${4:-}"; SEVENZ="${5:-}"
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="$(mktemp -d)"
ROOT="$WORK/root"
mkdir -p "$ROOT/Library/Audio/Plug-Ins/VST3" "$ROOT/Library/Audio/Plug-Ins/Components" \
         "$ROOT/Library/Audio/Plug-Ins/CLAP" "$ROOT/Applications"
cp -R "$A/VST3/SubRoo Rooms.vst3" "$ROOT/Library/Audio/Plug-Ins/VST3/"
cp -R "$A/AU/SubRoo Rooms.component" "$ROOT/Library/Audio/Plug-Ins/Components/"
cp -R "$A/CLAP/SubRoo Rooms.clap" "$ROOT/Library/Audio/Plug-Ins/CLAP/"
cp -R "$A/Standalone/SubRoo Rooms.app" "$ROOT/Applications/"
for b in "$ROOT"/Library/Audio/Plug-Ins/*/* "$ROOT/Applications/SubRoo Rooms.app"; do codesign --force --deep -s - "$b"; done

# scripts: postinstall with the pack values filled in + the 7-Zip decoder
mkdir -p "$WORK/scripts"
cp "$HERE/scripts/postinstall" "$WORK/scripts/postinstall"
PV=$(grep '^#define PackVersion ' "$HERE/../windows/packs.iss" | sed 's/.*"\(.*\)"/\1/')
PF=$(grep '^#define PackFile ' "$HERE/../windows/packs.iss" | sed 's/.*"\(.*\)"/\1/')
PB=$(grep '^#define PackBaseUrl ' "$HERE/../windows/packs.iss" | sed 's/.*"\(.*\)"/\1/')
PS=$(grep '^#define PackSha256Value ' "$HERE/../windows/packs.iss" | sed 's/.*"\(.*\)"/\1/')
if [ -n "$KEY" ] && [ -n "$SEVENZ" ]; then
    sed -i '' -e "s|@@PACK_VERSION@@|$PV|" -e "s|@@PACK_URL@@|$PB$PF|" -e "s|@@PACK_SHA256@@|$PS|" -e "s|@@PACK_KEY@@|$KEY|" "$WORK/scripts/postinstall"
    cp "$SEVENZ" "$WORK/scripts/7zz"; chmod +x "$WORK/scripts/7zz"
fi
chmod +x "$WORK/scripts/postinstall"

# bundles must not be "relocated" to an older copy somewhere else on disk
pkgbuild --analyze --root "$ROOT" "$WORK/components.plist"
i=0
while /usr/libexec/PlistBuddy -c "Print :$i" "$WORK/components.plist" >/dev/null 2>&1; do
    /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$WORK/components.plist" 2>/dev/null || \
        /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$WORK/components.plist"
    /usr/libexec/PlistBuddy -c "Set :$i:BundleOverwriteAction upgrade" "$WORK/components.plist" 2>/dev/null || \
        /usr/libexec/PlistBuddy -c "Add :$i:BundleOverwriteAction string upgrade" "$WORK/components.plist"
    i=$((i+1))
done

pkgbuild --root "$ROOT" --component-plist "$WORK/components.plist" --scripts "$WORK/scripts" \
         --identifier com.subroo.rooms.pkg --version "$VER" --install-location / "$WORK/SubRooRooms.pkg"

cat > "$WORK/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>SubRoo Rooms $VER</title>
    <welcome file="welcome.html" mime-type="text/html"/>
    <conclusion file="conclusion.html" mime-type="text/html"/>
    <options customize="never" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
    <volume-check><allowed-os-versions><os-version min="10.15"/></allowed-os-versions></volume-check>
    <choices-outline><line choice="default"><line choice="com.subroo.rooms.pkg"/></line></choices-outline>
    <choice id="default"/>
    <choice id="com.subroo.rooms.pkg" visible="false"><pkg-ref id="com.subroo.rooms.pkg"/></choice>
    <pkg-ref id="com.subroo.rooms.pkg" version="$VER" onConclusion="none">SubRooRooms.pkg</pkg-ref>
</installer-gui-script>
XML
mkdir -p "$WORK/res"
cat > "$WORK/res/welcome.html" <<HTML
<html><body style="font-family:-apple-system,Helvetica;font-size:13px">
<h2>SubRoo Rooms $VER</h2>
<p>Installs the <b>AU</b>, <b>VST3</b> and <b>CLAP</b> plug-ins, the <b>standalone app</b> and downloads all measured location packs.</p>
<p>An older version is updated in place. Please close your DAW before continuing.</p>
</body></html>
HTML
cat > "$WORK/res/conclusion.html" <<HTML
<html><body style="font-family:-apple-system,Helvetica;font-size:13px">
<h2>Done</h2><p>Rescan plug-ins in your DAW if SubRoo Rooms does not show up right away (listed under SubRoo).</p>
</body></html>
HTML
mkdir -p "$OUT"
productbuild --distribution "$WORK/distribution.xml" --resources "$WORK/res" --package-path "$WORK" \
             "$OUT/SubRoo-Rooms-$VER-macOS.pkg"
rm -rf "$WORK"
echo "built $OUT/SubRoo-Rooms-$VER-macOS.pkg"
