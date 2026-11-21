#!/bin/sh
# mhs35-layout.sh — autodetecting display layout for the MHS35 fix.
#
# Computes the desktop layout from what is ACTUALLY connected, at
# the moment it runs:
#
#   * every HDMI/external output at its own preferred mode
#     (whatever brand or resolution it reports), in port order
#   * the SPI panel last, rotated (transform 90), at whatever x
#     the real screens add up to — or at 0,0 when it is the only
#     screen, so a panel-only boot is a normal, usable desktop
#   * the taskbar (wf-panel-pi) targeted at the first external
#     output, or at the panel when no external output exists.
#     The script only records that target in the panel's config —
#     it NEVER restarts the panel mid-session. Hardware finding
#     (2026-11-21): a wf-panel-pi process that starts after the
#     BothScreens virtual output exists cannot draw its bar on an
#     external screen (its notifications appear there; the bar
#     itself vanishes), while the login instance — which starts
#     before the virtual output is created — lands correctly.
#     So the bar is placed at boot: plug everything in before
#     booting. A mid-session plug-in still gets its layout and
#     Pi Connect view adjusted; the bar stays where it booted
#     until the next reboot.
#   * the "BothScreens" virtual output sized to the real bounding
#     box of the layout, with Pi Connect's wayvnc pointed at it
#
# Nothing here names a specific monitor, brand, resolution or
# port. A missing or different main screen is a normal case, not
# an error: whatever is plugged in gets laid out, and the panel
# always comes up usable. (The previous static kanshi profiles
# pinned one desk monitor at 1024x768; booting away from that
# monitor hung the Pi at a white screen.)
#
# The script only writes settings that differ from the computed
# layout, so re-running it on an already-correct desktop changes
# nothing and generates no compositor churn. It is run at login
# and on display changes by mhs35-layout-watch.sh, and is safe to
# run by hand at any time. Every run is logged to
# ~/.local/state/mhs35-layout.log; failures are logged as FAIL
# lines, never silently. Exit status is always 0 — a layout
# problem must never block the session.
#
# Usage: mhs35-layout.sh [settle-delay-seconds] [phase]
#   phase = all (default) | screens | connect
# "screens" does only the physical layout + taskbar; "connect"
# does only the virtual output + Pi Connect capture. The watcher
# runs screens first at login (the desktop is correct as fast as
# possible) and connect a few seconds later — Pi Connect cannot
# be used before the Pi has booted anyway, so nothing is gained
# by making the screens wait for it.

DELAY=${1:-2}
PHASE=${2:-all}
STATE_DIR="$HOME/.local/state"
LOG="$STATE_DIR/mhs35-layout.log"
WORK="$STATE_DIR/layout-work"
mkdir -p "$WORK"
exec >> "$LOG" 2>&1
echo "--- layout run at $(date) (delay ${DELAY}s, phase $PHASE) ---"

# One run at a time: the watcher, autostart and manual runs can
# race. A lock older than 120 s is stale (a run never takes that
# long) and is stolen.
LOCK="$STATE_DIR/mhs35-layout.lock"
if ! mkdir "$LOCK" 2>/dev/null; then
  NOW=$(date +%s)
  THEN=$(stat -c %Y "$LOCK" 2>/dev/null || echo 0)
  if [ $((NOW - THEN)) -gt 120 ]; then
    rmdir "$LOCK" 2>/dev/null
    mkdir "$LOCK" 2>/dev/null \
      || { echo "another layout run is active; exiting"; exit 0; }
  else
    echo "another layout run is active; exiting"
    exit 0
  fi
fi
trap 'rmdir "$LOCK" 2>/dev/null' EXIT INT TERM

sleep "$DELAY"

command -v wlr-randr >/dev/null 2>&1 \
  || { echo "FAIL: wlr-randr not found"; exit 0; }

CTL="$XDG_RUNTIME_DIR/rpi-connect-wayvnc-ctl.sock"

# --- plan ------------------------------------------------------------
# Read wlr-randr's output and emit a plan:
#   APPLY name mode pos transform  physical output needs a write
#   OK name                        already exactly as desired
#   SKIP name                      no usable mode; left alone
#   BBOX w h                       bounding box of the layout
#   TASKBAR name                   output the taskbar belongs on
#   PANELX x                       desired panel position
#   VIRT name curw curh posx posy  virtual output state
#   VIRTNONE                       no virtual output yet
plan() {
  wlr-randr 2>/dev/null | awk '
    function flush() {
      if (name == "") return
      if (prefW == 0) { prefW = bestW; prefH = bestH }
      cls = "ext"
      if (name ~ /^SPI-/) cls = "panel"
      else if (name == "BothScreens" || name ~ /^HEADLESS-/) cls = "virt"
      n++
      o_name[n] = name; o_cls[n] = cls; o_en[n] = enabled
      o_pw[n] = prefW; o_ph[n] = prefH
      o_cw[n] = curW;  o_ch[n] = curH
      o_px[n] = posX;  o_py[n] = posY
      o_tr[n] = trans; o_sc[n] = scale
    }
    /^[^ \t]/ {
      flush()
      name = $1; enabled = 0; inw = 0
      curW = curH = prefW = prefH = bestW = bestH = bestA = 0
      posX = posY = 0; trans = "normal"; scale = 1
      next
    }
    /^[ \t]+Enabled:/ { enabled = ($2 == "yes"); next }
    /^[ \t]+Modes:/ { inw = 1; next }
    /^[ \t]+Position:/ {
      split($2, pp, ","); posX = pp[1] + 0; posY = pp[2] + 0
      inw = 0; next
    }
    /^[ \t]+Transform:/ { trans = $2; inw = 0; next }
    /^[ \t]+Scale:/ { scale = $2 + 0; inw = 0; next }
    inw && /^[ \t]+[0-9]+x[0-9]+ px/ {
      split($1, mm, "x"); w = mm[1] + 0; h = mm[2] + 0
      if ($0 ~ /preferred/) { prefW = w; prefH = h }
      if ($0 ~ /current/)   { curW = w;  curH = h }
      if (w * h > bestA)    { bestA = w * h; bestW = w; bestH = h }
      next
    }
    /^[ \t]/ { inw = 0; next }
    END {
      flush()
      # Order: HDMI-A-1, HDMI-A-2, other externals by name,
      # panel last. (Tiny n — insertion sort.)
      m = 0
      for (i = 1; i <= n; i++) {
        if (o_cls[i] == "virt") continue
        if (o_name[i] == "HDMI-A-1") k = "1"
        else if (o_name[i] == "HDMI-A-2") k = "2"
        else if (o_cls[i] == "panel") k = "9"
        else k = "3" o_name[i]
        m++; key[m] = k; idx[m] = i
      }
      for (a = 2; a <= m; a++) {
        kk = key[a]; ii = idx[a]; b = a - 1
        while (b >= 1 && key[b] > kk) {
          key[b + 1] = key[b]; idx[b + 1] = idx[b]; b--
        }
        key[b + 1] = kk; idx[b + 1] = ii
      }
      x = 0; maxh = 0; taskbar = ""
      for (a = 1; a <= m; a++) {
        i = idx[a]
        if (o_pw[i] == 0) { printf "SKIP %s\n", o_name[i]; continue }
        if (o_cls[i] == "panel") {
          dtr = "90"; lw = o_ph[i]; lh = o_pw[i]
        } else {
          dtr = "normal"; lw = o_pw[i]; lh = o_ph[i]
        }
        if (taskbar == "" && o_cls[i] != "panel") taskbar = o_name[i]
        mode = o_pw[i] "x" o_ph[i]
        curmode = o_cw[i] "x" o_ch[i]
        if (!o_en[i] || curmode != mode || o_tr[i] != dtr \
            || o_sc[i] != 1 || o_px[i] != x || o_py[i] != 0)
          printf "APPLY %s %s %d,0 %s\n", o_name[i], mode, x, dtr
        else
          printf "OK %s\n", o_name[i]
        if (o_cls[i] == "panel") printf "PANELX %d\n", x
        x += lw; if (lh > maxh) maxh = lh
      }
      if (taskbar == "") {
        for (i = 1; i <= n; i++)
          if (o_cls[i] == "panel") taskbar = o_name[i]
      }
      printf "BBOX %d %d\n", x, maxh
      if (taskbar != "") printf "TASKBAR %s\n", taskbar
      vfound = 0
      for (i = 1; i <= n; i++) {
        if (o_cls[i] == "virt") {
          printf "VIRT %s %d %d %d %d\n", \
            o_name[i], o_cw[i], o_ch[i], o_px[i], o_py[i]
          vfound = 1
        }
      }
      if (!vfound) print "VIRTNONE"
    }
  '
}

apply_plan() {
  # $1 = plan file. Runs one wlr-randr write per APPLY line.
  while read -r cmd name mode pos tr; do
    [ "$cmd" = "APPLY" ] || continue
    echo "setting $name: mode $mode, position $pos, transform $tr"
    wlr-randr --output "$name" --on --mode "$mode" --pos "$pos" \
      --transform "$tr" --scale 1 2>/dev/null \
      || echo "FAIL: wlr-randr could not set $name"
  done < "$1"
}

field() { # $1 = plan file, $2 = key, $3 = field number
  awk -v k="$2" -v f="$3" '$1 == k { print $f }' "$1"
}

plan > "$WORK/plan.txt"
if [ "$PHASE" != "connect" ]; then
  apply_plan "$WORK/plan.txt"
  grep '^SKIP' "$WORK/plan.txt" | while read -r _ name; do
    echo "SKIP: $name reports no usable mode; left as-is"
  done
fi

BBOX_W=$(field "$WORK/plan.txt" BBOX 2)
BBOX_H=$(field "$WORK/plan.txt" BBOX 3)
TASKBAR=$(field "$WORK/plan.txt" TASKBAR 2)

# --- taskbar ---------------------------------------------------------
# wf-panel-pi's monitor= names the output the taskbar lives on:
# the first external output, or the panel when there is none.
# Only the config is maintained here — the running panel is
# NEVER killed or restarted by this script. A panel process
# restarted mid-session (after the virtual output exists) loses
# its bar entirely when aimed at an external screen (verified on
# hardware 2026-11-21), so a restart trades a working bar on the
# "wrong" screen for no bar at all. Keeping the config correct
# means the next login — whose panel starts before the virtual
# output is created — places the bar correctly.
if [ "$PHASE" != "connect" ] && [ -n "$TASKBAR" ]; then
  PANEL_INI="$HOME/.config/wf-panel-pi/wf-panel-pi.ini"
  [ -f "$PANEL_INI" ] || PANEL_INI="$HOME/.config/wf-panel-pi.ini"
  CUR=""
  [ -f "$PANEL_INI" ] \
    && CUR=$(grep '^monitor=' "$PANEL_INI" | head -1 | cut -d= -f2)
  if [ "$CUR" != "$TASKBAR" ]; then
    if [ -f "$PANEL_INI" ] && grep -q '^monitor=' "$PANEL_INI"; then
      sed -i "s/^monitor=.*/monitor=$TASKBAR/" "$PANEL_INI"
    elif [ -f "$PANEL_INI" ] && grep -q '^\[panel\]' "$PANEL_INI"; then
      sed -i "s/^\[panel\]/[panel]\nmonitor=$TASKBAR/" "$PANEL_INI"
    else
      mkdir -p "$(dirname "$PANEL_INI")"
      printf '[panel]\nmonitor=%s\n' "$TASKBAR" > "$PANEL_INI"
    fi
    echo "taskbar target set to $TASKBAR (applies at next login)"
  fi
fi

# --- virtual output (Pi Connect shows the whole layout) --------------
if [ "$PHASE" != "screens" ]; then
VIRT_PRESENT=1
if grep -q '^VIRTNONE' "$WORK/plan.txt"; then
  VIRT_PRESENT=0
  if command -v wtype >/dev/null 2>&1; then
    # Super+V keybind (installed in labwc rc.xml by the setup).
    wtype -M logo -P v -m logo 2>/dev/null
    sleep 2
    plan > "$WORK/plan.txt"
    grep -q '^VIRTNONE' "$WORK/plan.txt" || VIRT_PRESENT=1
  else
    echo "FAIL: wtype is not installed — cannot create the" \
         "virtual output. Install: sudo apt install -y wtype"
  fi
  if [ "$VIRT_PRESENT" -eq 0 ]; then
    echo "FAIL: virtual output was not created; Pi Connect will" \
         "show a single screen this session"
  fi
fi

if [ "$VIRT_PRESENT" -eq 1 ]; then
  VW=$(field "$WORK/plan.txt" VIRT 3)
  VH=$(field "$WORK/plan.txt" VIRT 4)
  VPX=$(field "$WORK/plan.txt" VIRT 5)
  VPY=$(field "$WORK/plan.txt" VIRT 6)
  if [ "$VW" != "$BBOX_W" ] || [ "$VH" != "$BBOX_H" ] \
     || [ "$VPX" != "0" ] || [ "$VPY" != "0" ]; then
    echo "sizing virtual output to ${BBOX_W}x${BBOX_H} at 0,0"
    wlr-randr --output BothScreens \
      --custom-mode "${BBOX_W}x${BBOX_H}" --pos 0,0 2>/dev/null \
      || echo "FAIL: could not size the virtual output"
    sleep 1
    # Creating/resizing the virtual output re-flows the physical
    # outputs (seen on hardware: the panel jumped outside the
    # frame). Re-plan from the new state and re-apply whatever
    # drifted — normally just positions.
    plan > "$WORK/plan2.txt"
    apply_plan "$WORK/plan2.txt"
  fi

  # Point Pi Connect's wayvnc at the virtual output. The control
  # socket can lag at login, so retry for ~30 s — but skip the
  # call entirely when BothScreens is already the captured one.
  if [ -S "$CTL" ]; then
    if wayvncctl --socket="$CTL" output-list 2>/dev/null \
         | grep 'BothScreens' | grep -q '\*'; then
      echo "Pi Connect already capturing BothScreens"
    else
      n=0
      while [ $n -lt 10 ]; do
        if wayvncctl --socket="$CTL" \
             output-set BothScreens 2>/dev/null; then
          echo "Pi Connect capture set to BothScreens"
          break
        fi
        n=$((n + 1))
        sleep 3
      done
      [ $n -ge 10 ] \
        && echo "FAIL: could not set Pi Connect capture ($CTL)"
    fi
  else
    echo "note: Pi Connect control socket not present; capture" \
         "will be set on a later run"
  fi
fi
fi # PHASE != screens

# --- verify ----------------------------------------------------------
plan > "$WORK/verify.txt"
if grep -q '^APPLY' "$WORK/verify.txt"; then
  echo "FAIL: layout did not converge; still to apply:"
  grep '^APPLY' "$WORK/verify.txt"
else
  echo "layout OK: ${BBOX_W}x${BBOX_H}, taskbar target ${TASKBAR:-none}"
fi

exit 0
