#!/bin/bash
# SubRoo Rooms - Linux installer (per user)
cd "$(dirname "$0")"
mkdir -p ~/.vst3 ~/.clap ~/.lv2 "$HOME/Documents/SubRoo Rooms/Packs"
cp -R "SubRoo Rooms.vst3" ~/.vst3/
cp "SubRoo Rooms.clap" ~/.clap/
cp -R "SubRoo Rooms.lv2" ~/.lv2/
echo "Done."
