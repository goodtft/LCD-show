#!/bin/sh
# mhs35-layout-watch.sh — keep the display layout matched to the
# hardware, all session long.
#
# Started once from ~/.config/labwc/autostart. At login it runs
# the layout script's screens phase immediately (detection +
# layout + taskbar) and its connect phase a few seconds later
# (virtual output + Pi Connect capture — useless before the Pi
# has booted anyway, so it never delays the screens). Then it
# polls the output list every 5 s and runs the full layout again
# whenever the connected displays change (monitor plugged in,
# unplugged, swapped). The layout script only writes what
# actually differs, so a trigger on an already-correct desktop
# is a no-op.
#
# kanshi is deliberately NOT used for this: its profiles are
# static (fixed modes and positions), which is exactly the
# assumption that white-screened the Pi when it booted away from
# its desk monitor. One dynamic writer, no fighting over the
# layout.

BIN="$HOME/.local/bin/mhs35-layout.sh"
STATE_DIR="$HOME/.local/state"
LOG="$STATE_DIR/mhs35-layout.log"
mkdir -p "$STATE_DIR"

fingerprint() {
  wlr-randr 2>/dev/null | cksum
}

echo "--- layout watcher started at $(date) ---" >> "$LOG"
# Screens first, immediately: detection + layout + taskbar, so
# the desktop is correct as fast as possible after login. The
# Connect/virtual-output phase runs a few seconds later — Pi
# Connect cannot be used before the Pi has booted anyway, so
# the screens never wait on it.
"$BIN" 0 screens
sleep 4
"$BIN" 0 connect
LAST=$(fingerprint)

while true; do
  sleep 5
  NOW=$(fingerprint)
  if [ "$NOW" != "$LAST" ]; then
    echo "--- display change detected at $(date) ---" >> "$LOG"
    "$BIN" 0 all
    LAST=$(fingerprint)
  fi
done
