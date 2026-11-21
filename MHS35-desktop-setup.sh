#!/bin/bash
# MHS35-desktop-setup.sh — user-side desktop setup for an
# MHS35-DRM-show install. Run as your normal user (NO sudo), from
# the repo directory, after the installer has run and rebooted:
#   ./MHS35-desktop-setup.sh
#
# It installs the autodetecting layout system:
#   * mhs35-layout.sh in ~/.local/bin — computes the layout from
#     whatever is actually connected (0, 1 or 2 HDMI outputs, any
#     brand/mode, plus the panel), puts the taskbar on the first
#     external screen (or the panel when there is none), and
#     sizes the "BothScreens" virtual output to the real layout
#     so Raspberry Pi Connect shows every screen at once
#   * mhs35-layout-watch.sh + a labwc autostart line — runs the
#     layout at login and whenever displays are plugged/unplugged
#   * Super+V keybind in labwc rc.xml creating the virtual output
#   * kanshi is given an empty (comment-only) config on purpose:
#     its static profiles were the desk-monitor assumption that
#     white-screened the Pi when it booted with different screens
# Nothing assumes a specific monitor exists; a panel-only boot
# is a normal, usable desktop. Existing files are backed up next
# to the originals (config.bak-<timestamp>). Safe to re-run;
# re-running also migrates an older (static-layout) install.

set -u
cd "$(dirname "$0")"
STAMP=$(date +%Y%m%d-%H%M%S)

backup() {
  if [ -f "$1" ]; then
    cp -a "$1" "$1.bak-$STAMP"
    echo "backed up $1 -> $1.bak-$STAMP"
  fi
}

# --- kanshi: intentionally no profiles ---
# Layout is computed dynamically by mhs35-layout.sh. kanshi's
# static profiles (fixed mode + position per connector) are what
# assumed the desk monitor exists; leave kanshi with nothing to
# apply so it can never fight the layout script.
KDIR="$HOME/.config/kanshi"
mkdir -p "$KDIR"
backup "$KDIR/config"
cat > "$KDIR/config" << 'EOF'
# No profiles on purpose. Display layout on this system is
# managed dynamically by mhs35-layout.sh (autodetect — see the
# LCD-show fork's FIXES.md). Static kanshi profiles assumed one
# specific monitor and white-screened boots without it.
EOF
echo "kanshi config set to no-profiles (dynamic layout is in charge)"
pkill -HUP kanshi 2>/dev/null || true

# The taskbar's output is managed by mhs35-layout.sh (first
# external screen, else the panel) — no static pin here.

# --- Super+V creates the virtual output ---
RC="$HOME/.config/labwc/rc.xml"
mkdir -p "$(dirname "$RC")"
if [ -f "$RC" ] && grep -q 'BothScreens' "$RC"; then
  echo "rc.xml already has the BothScreens keybind"
elif [ -f "$RC" ]; then
  backup "$RC"
  sed -i 's|</openbox_config>|  <keyboard>\n    <keybind key="W-v">\n      <action name="VirtualOutputAdd" output_name="BothScreens"/>\n    </keybind>\n  </keyboard>\n</openbox_config>|' "$RC"
  echo "added Super+V keybind to $RC"
else
  printf '<?xml version="1.0"?>\n<openbox_config xmlns="http://openbox.org/3.4/rc">\n  <keyboard>\n    <keybind key="W-v">\n      <action name="VirtualOutputAdd" output_name="BothScreens"/>\n    </keybind>\n  </keyboard>\n</openbox_config>\n' > "$RC"
  echo "created $RC with the Super+V keybind"
fi
killall -SIGHUP labwc 2>/dev/null || true

# --- dependency: wtype (triggers the virtual output) ---
# Learned on hardware 2026-11-19: wtype is NOT in the default
# install, and without it the layout script cannot create the
# virtual output. Install it here so a fresh setup just works.
if ! command -v wtype >/dev/null 2>&1; then
  echo "installing wtype (needed to auto-create the virtual screen)..."
  sudo apt-get install -y wtype \
    || echo "WARNING: wtype install failed — virtual screen will" \
            "not auto-create until 'sudo apt install -y wtype' succeeds"
fi

# --- layout scripts + autostart ---
mkdir -p "$HOME/.local/bin"
cp -f ./usr/mhs35-layout.sh ./usr/mhs35-layout-watch.sh "$HOME/.local/bin/"
chmod +x "$HOME/.local/bin/mhs35-layout.sh" \
         "$HOME/.local/bin/mhs35-layout-watch.sh"
echo "installed mhs35-layout.sh + mhs35-layout-watch.sh -> ~/.local/bin"
# Migrate from the previous generation: the bothscreens login
# script is superseded by the layout script (which also handles
# the virtual output, sized to the real layout).
if [ -f "$HOME/.local/bin/mhs35-bothscreens.sh" ]; then
  rm -f "$HOME/.local/bin/mhs35-bothscreens.sh"
  echo "removed superseded mhs35-bothscreens.sh"
fi
AUTOSTART="$HOME/.config/labwc/autostart"
touch "$AUTOSTART"
if grep -q 'mhs35-bothscreens' "$AUTOSTART"; then
  backup "$AUTOSTART"
  sed -i '/mhs35-bothscreens/d' "$AUTOSTART"
  echo "removed mhs35-bothscreens.sh from labwc autostart"
fi
if grep -q 'mhs35-layout-watch' "$AUTOSTART"; then
  echo "autostart already runs mhs35-layout-watch.sh"
else
  backup "$AUTOSTART"
  echo '$HOME/.local/bin/mhs35-layout-watch.sh &' >> "$AUTOSTART"
  echo "added mhs35-layout-watch.sh to labwc autostart"
fi

# --- apply now, not just at next login ---
"$HOME/.local/bin/mhs35-layout.sh" 0
echo "done. (layout log: ~/.local/state/mhs35-layout.log)"
echo "Reboot once to verify everything comes back on its own —"
echo "including with different screens, or none but the panel."
