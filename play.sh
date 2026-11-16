#!/bin/sh
# Build the playable OpenLara (no bench harness), stage it on the iPod and launch it
# through the USB RAM launcher at stock clocks. The iPod must be connected in Disk Mode
# with NanoApps (as for hwbridge/run_native.py) and the level data in
# /Apps/Data/OpenLara/DATA (LEVEL2.PHD).
#
# The game runs in landscape (hold the iPod with the home button on the right; build with
# --fixture-cppflags="-DLARA_LANDSCAPE=0" for portrait). Touch: left third = move stick,
# middle = look, right = action buttons (OpenLara's own touch layout, now laid out landscape).
#
# The launcher waits ~2 minutes for a bench report and then says "no fresh report":
# that is expected here, the game keeps running on the iPod. Press Home to quit.
set -e
NANO=${NANO:-/Users/jwalr/nano-gles2}
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$NANO"
touch NanoOS/src/main.c   # unique build tag for the launcher's freshness check
KERNEL_DEFS="-DNANOOS_GAME_PROFILE=0 -DMBX_PFX_EVERY_SCENE" \
  python3 bundle_native.py --nand-boot openlara --fixture-cppflags="" --out hwbridge/build/payload.bin >/dev/null
cp "$HERE/build/game.bin" /Volumes/iPod/Apps/Data/Game/game.bin
sync
OUT="${TMPDIR:-/tmp}/openlara-play-$(date +%s).txt"
python3 hwbridge/run_native.py --out "$OUT" || echo "Launched; the game is running on the iPod (no bench report is expected)."
