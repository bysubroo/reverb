#!/bin/bash
# SubRoo Rooms - macOS installer (double click; the plug-ins are not notarised)
cd "$(dirname "$0")"
P="$HOME/Library/Audio/Plug-Ins"
mkdir -p "$P/VST3" "$P/Components" "$P/CLAP" "$HOME/Documents/SubRoo Rooms/Packs"
rm -rf "$P/VST3/SubRoo Rooms.vst3" "$P/Components/SubRoo Rooms.component" "$P/CLAP/SubRoo Rooms.clap"
cp -R "SubRoo Rooms.vst3" "$P/VST3/"
cp -R "SubRoo Rooms.component" "$P/Components/"
cp -R "SubRoo Rooms.clap" "$P/CLAP/"
[ -d "SubRoo Rooms.app" ] && cp -R "SubRoo Rooms.app" /Applications/ 2>/dev/null
xattr -dr com.apple.quarantine "$P/VST3/SubRoo Rooms.vst3" "$P/Components/SubRoo Rooms.component" "$P/CLAP/SubRoo Rooms.clap" "/Applications/SubRoo Rooms.app" 2>/dev/null
killall -9 AudioComponentRegistrar 2>/dev/null
echo "Done. Restart your DAW (Logic: Settings > Plug-in Manager > Reset & Rescan if needed)."
