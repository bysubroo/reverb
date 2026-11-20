#!/bin/bash
# Installs the .pkg on the CI runner and checks plug-ins + packs.
set -e
sudo installer -pkg "$1" -target /
sudo cat "/Library/Logs/SubRoo Rooms Install.log" || true
for f in "/Library/Audio/Plug-Ins/VST3/SubRoo Rooms.vst3" "/Library/Audio/Plug-Ins/Components/SubRoo Rooms.component" \
         "/Library/Audio/Plug-Ins/CLAP/SubRoo Rooms.clap" "/Applications/SubRoo Rooms.app" \
         "/Library/Application Support/SubRoo Rooms/Packs/.packs-version"; do
    [ -e "$f" ] || { echo "::error::missing $f"; exit 1; }
done
n=$(find "/Library/Application Support/SubRoo Rooms/Packs" -mindepth 1 -maxdepth 1 -type d | wc -l | tr -d ' ')
echo "packs installed: $n"; [ "$n" -ge 200 ]
# second install = update in place, packs must not be downloaded again
before=$(stat -f %m "/Library/Application Support/SubRoo Rooms/Packs/.packs-version")
sudo installer -pkg "$1" -target /
after=$(stat -f %m "/Library/Application Support/SubRoo Rooms/Packs/.packs-version")
[ "$before" = "$after" ] || { echo "::error::packs reinstalled although up to date"; exit 1; }
echo "macOS install / update in place OK"
