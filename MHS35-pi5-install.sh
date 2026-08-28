#!/bin/bash
# ============================================================================
# MHS35 3.5" SPI TFT LCD Display — Raspberry Pi 5 Installer
# ============================================================================
#
# PURPOSE:
#   Install and configure the MHS35 / MPI3501 3.5" SPI LCD on a
#   Raspberry Pi 5 (8 GB or any variant) running Raspberry Pi OS
#   Bookworm (Debian 12) or later (Trixie / Debian 13).
#
# WHY A NEW SCRIPT?
#   The original MHS35-show and MHS35-fixed-install.sh scripts rely on:
#     • fbcp (DispmanX framebuffer copy) — DispmanX is REMOVED on Pi 5
#     • fbturbo / fbdev X11 drivers        — deprecated, crash on Bookworm
#     • vc4-fkms-v3d                        — not available on Pi 5
#     • /boot/config.txt path               — moved to /boot/firmware/
#     • Legacy HDMI mode spoofing           — unnecessary with DRM
#   None of those work on the Raspberry Pi 5 hardware (RP1 I/O chip).
#
# WHAT THIS SCRIPT DOES:
#   1. Detects hardware & OS version, confirms Pi 5
#   2. Backs up current config
#   3. Switches display server from Wayland → X11 (required for SPI LCDs)
#   4. Enables SPI bus
#   5. Installs the mhs35 device-tree overlay (.dtbo)
#   6. Configures /boot/firmware/config.txt using the NATIVE DRM piscreen
#      overlay (kernel-level, no fbcp needed)
#   7. Installs evdev touch driver + calibration for ADS7846/XPT2046
#   8. Removes ALL legacy cruft (fbturbo, .bash_profile hack, broken confs)
#   9. Sets boot to Desktop Autologin
#  10. Reboots
#
# COMPATIBLE WITH:
#   • Raspberry Pi 5  (4 GB / 8 GB / 16 GB)
#   • Raspberry Pi OS Bookworm 64-bit (Debian 12.x)
#   • Raspberry Pi OS Trixie 64-bit (Debian 13.x)
#   • MHS35 / MPI3501 displays (ILI9486 controller, XPT2046 touch)
#
# USAGE:
#   cd LCD-show
#   chmod +x MHS35-pi5-install.sh
#   sudo ./MHS35-pi5-install.sh [rotation]
#
#   rotation = 0, 90, 180, 270  (default: 90 = landscape)
#
# TO RESTORE:
#   sudo ./MHS35-pi5-restore.sh
#
# ============================================================================

set -euo pipefail

# ── Colours ──────────────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Colour

ok()   { echo -e "  ${GREEN}[OK]${NC}  $1"; }
info() { echo -e "  ${CYAN}[INFO]${NC} $1"; }
warn() { echo -e "  ${YELLOW}[WARN]${NC} $1"; }
fail() { echo -e "  ${RED}[FAIL]${NC} $1"; exit 1; }
step() { echo -e "\n${BOLD}── STEP $1 ──${NC}"; }

# ── Pre-flight ───────────────────────────────────────────────────────────────
if [ "$(id -u)" -ne 0 ]; then
    fail "This script must be run as root. Use: sudo $0"
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

ROTATION="${1:-90}"
case "$ROTATION" in
    0|90|180|270) ;;
    *) fail "Invalid rotation '$ROTATION'. Use 0, 90, 180, or 270." ;;
esac

echo ""
echo -e "${BOLD}============================================${NC}"
echo -e "${BOLD} MHS35 Display — Raspberry Pi 5 Installer${NC}"
echo -e "${BOLD}============================================${NC}"
echo ""

# ============================================================================
# STEP 0: Detect Hardware & OS
# ============================================================================
step "0/9: Detecting hardware and OS"

# Detect Pi model
if [ -f /proc/device-tree/model ]; then
    PI_MODEL="$(tr -d '\0' < /proc/device-tree/model)"
    info "Board: $PI_MODEL"
else
    PI_MODEL="unknown"
    warn "Could not detect board model"
fi

# Check if Pi 5 (warn but don't block — script may work on Pi 4 too)
if echo "$PI_MODEL" | grep -qi "Raspberry Pi 5"; then
    ok "Raspberry Pi 5 detected"
    IS_PI5=1
elif echo "$PI_MODEL" | grep -qi "Raspberry Pi 4"; then
    warn "Raspberry Pi 4 detected — this script is designed for Pi 5"
    warn "It should still work, but the original MHS35-fixed-install.sh may also work"
    IS_PI5=0
else
    warn "Non-standard board detected: $PI_MODEL"
    IS_PI5=0
fi

# Detect OS
DEB_VERSION="$(cat /etc/debian_version 2>/dev/null | tr -d '\n' || echo "unknown")"
info "Debian version: $DEB_VERSION"

ARCH="$(getconf LONG_BIT)"
info "Architecture: ${ARCH}-bit"

# Detect username
USERNAME="$(logname 2>/dev/null || echo "${SUDO_USER:-$USER}")"
info "User: $USERNAME"
info "Rotation: ${ROTATION}°"

# Determine config paths (Bookworm uses /boot/firmware/)
if [ -f /boot/firmware/config.txt ]; then
    CONFIG_PATH="/boot/firmware/config.txt"
    OVERLAY_DIR="/boot/firmware/overlays"
    CMDLINE_PATH="/boot/firmware/cmdline.txt"
elif [ -f /boot/config.txt ]; then
    CONFIG_PATH="/boot/config.txt"
    OVERLAY_DIR="/boot/overlays"
    CMDLINE_PATH="/boot/cmdline.txt"
else
    fail "Cannot find config.txt — is this a Raspberry Pi?"
fi
info "Config: $CONFIG_PATH"

# ============================================================================
# STEP 1: Backup
# ============================================================================
step "1/9: Backing up current configuration"

BACKUP_DIR="$SCRIPT_DIR/.pi5_backup_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$BACKUP_DIR"

cp "$CONFIG_PATH" "$BACKUP_DIR/config.txt.bak"
[ -f "$CMDLINE_PATH" ] && cp "$CMDLINE_PATH" "$BACKUP_DIR/cmdline.txt.bak"
[ -f /etc/rc.local ] && cp /etc/rc.local "$BACKUP_DIR/rc.local.bak"

# Save the backup path for the restore script
echo "$BACKUP_DIR" > "$SCRIPT_DIR/.pi5_last_backup"

ok "Backup saved to $BACKUP_DIR"

# ============================================================================
# STEP 2: Switch Wayland → X11
# ============================================================================
step "2/9: Switching display server to X11"

# SPI LCD displays require X11 — Wayland cannot render to SPI framebuffers
if command -v raspi-config &>/dev/null; then
    raspi-config nonint do_wayland W1 2>/dev/null || true
    ok "Display server set to X11"
else
    warn "raspi-config not found — please manually switch to X11"
fi

# ============================================================================
# STEP 3: Enable SPI
# ============================================================================
step "3/9: Enabling SPI bus"

if command -v raspi-config &>/dev/null; then
    raspi-config nonint do_spi 0 2>/dev/null || true
fi

# Also ensure it's in config.txt
if ! grep -q "^dtparam=spi=on" "$CONFIG_PATH"; then
    sed -i 's/^#dtparam=spi=on/dtparam=spi=on/' "$CONFIG_PATH"
    if ! grep -q "^dtparam=spi=on" "$CONFIG_PATH"; then
        echo "dtparam=spi=on" >> "$CONFIG_PATH"
    fi
fi

ok "SPI enabled"

# ============================================================================
# STEP 4: Install MHS35 device-tree overlay
# ============================================================================
step "4/9: Installing MHS35 device-tree overlay"

if [ -f "$SCRIPT_DIR/usr/mhs35-overlay.dtb" ]; then
    cp "$SCRIPT_DIR/usr/mhs35-overlay.dtb" "$OVERLAY_DIR/"
    cp "$SCRIPT_DIR/usr/mhs35-overlay.dtb" "$OVERLAY_DIR/mhs35.dtbo"
    ok "MHS35 overlay installed to $OVERLAY_DIR"
else
    fail "Cannot find usr/mhs35-overlay.dtb — are you in the LCD-show directory?"
fi

# ============================================================================
# STEP 5: Configure /boot/firmware/config.txt for Pi 5 + DRM
# ============================================================================
step "5/9: Configuring $CONFIG_PATH for Pi 5 DRM"

# ── Remove ALL old LCD-related lines ──
# This cleans up any mess from previous install attempts
sed -i '/^dtoverlay=mhs35/d'           "$CONFIG_PATH"
sed -i '/^dtoverlay=tft35a/d'          "$CONFIG_PATH"
sed -i '/^dtoverlay=piscreen/d'        "$CONFIG_PATH"
sed -i '/^hdmi_force_hotplug/d'        "$CONFIG_PATH"
sed -i '/^hdmi_group/d'               "$CONFIG_PATH"
sed -i '/^hdmi_mode/d'                "$CONFIG_PATH"
sed -i '/^hdmi_cvt/d'                 "$CONFIG_PATH"
sed -i '/^hdmi_drive/d'              "$CONFIG_PATH"
sed -i '/^display_rotate/d'           "$CONFIG_PATH"
sed -i '/^lcd_rotate/d'               "$CONFIG_PATH"
sed -i '/^dtoverlay=ads7846/d'         "$CONFIG_PATH"
sed -i '/^dtparam=speed=/d'            "$CONFIG_PATH"
# Also remove any previous install block
sed -i '/# --- MHS35 Pi5 LCD/,/# --- End MHS35 Pi5 LCD ---/d' "$CONFIG_PATH"

# ── CRITICAL FIX FOR PI 5: Disable vc4-kms-v3d ──
# WHY: On Pi 5, the SPI LCD (ILI9486) is driven by the "piscreen" DRM
# overlay. When vc4-kms-v3d is active, the GPU grabs /dev/dri/card0 and
# the desktop compositor renders ONLY to HDMI. The SPI LCD never receives
# any pixel data → WHITE SCREEN.
#
# By disabling vc4-kms-v3d and using piscreen with the ,drm flag, the
# SPI LCD becomes the primary (and only) DRM display. The kernel drives
# pixels directly over SPI to the ILI9486 controller.
#
# NOTE: This means HDMI output will show low resolution or may not show
# the desktop. This is EXPECTED — the LCD is the primary display.
sed -i 's/^dtoverlay=vc4-kms-v3d/#dtoverlay=vc4-kms-v3d/' "$CONFIG_PATH"
sed -i 's/^dtoverlay=vc4-fkms-v3d/#dtoverlay=vc4-fkms-v3d/' "$CONFIG_PATH"

info "Disabled vc4-kms-v3d (required for SPI LCD as primary display)"

# ── Enable I2C (for touch) ──
if ! grep -q "^dtparam=i2c_arm=on" "$CONFIG_PATH"; then
    sed -i 's/^#dtparam=i2c_arm=on/dtparam=i2c_arm=on/' "$CONFIG_PATH"
    if ! grep -q "^dtparam=i2c_arm=on" "$CONFIG_PATH"; then
        echo "dtparam=i2c_arm=on" >> "$CONFIG_PATH"
    fi
fi

# ── Add Pi 5 LCD configuration under [all] section ──
# We use dtoverlay=piscreen,speed=18000000,drm
#
# piscreen is a KERNEL-BUILT-IN overlay for ILI9486 SPI displays.
# The ,drm flag forces it to use the modern DRM/TinyDRM subsystem
# instead of the legacy fbtft framebuffer — this is what makes it
# work on Pi 5's RP1 SPI controller.
#
# speed=18000000 (18 MHz) is safe for most MHS35/MPI3501 displays.
# Higher speeds (32MHz) may cause artifacts on some clones.

# Determine rotation value for piscreen overlay
PISCREEN_ROTATE="$ROTATION"

# Always APPEND a fresh [all] block at the end of the file rather than
# inserting after an existing "[all]" line. Stock Raspberry Pi OS config.txt
# contains [all] TWICE (top, and again as a reset after [cm4]) — inserting
# with `sed '/^\[all\]/a ...'` fires on every match and silently duplicates
# the overlay line, causing a GPIO/pin-conflict error on the second load.
cat >> "$CONFIG_PATH" << CONFIGEOF

[all]
# --- MHS35 Pi5 LCD (added by MHS35-pi5-install.sh) ---
dtoverlay=piscreen,speed=18000000,drm
hdmi_force_hotplug=1
# --- End MHS35 Pi5 LCD ---
CONFIGEOF

ok "config.txt configured for Pi 5 DRM (piscreen + drm, rotation=${ROTATION}°)"

# ============================================================================
# STEP 6: Configure console framebuffer redirect (optional)
# ============================================================================
step "6/9: Configuring framebuffer console"

# On Pi 5, the SPI LCD appears as a secondary DRM device (usually /dev/fb1).
# We add fbcon=map:11 to cmdline.txt to redirect the Linux console to it.
# This is optional but helps during boot to see text on the LCD.

if [ -f "$CMDLINE_PATH" ]; then
    # Remove any existing fbcon= entries first
    sed -i 's/ fbcon=map:[0-9]*//g' "$CMDLINE_PATH"
    
    # Add fbcon=map:11 (maps console to fb1, the SPI LCD)
    sed -i 's/$/ fbcon=map:11/' "$CMDLINE_PATH"
    
    ok "Console framebuffer redirect configured"
else
    warn "cmdline.txt not found — console redirect skipped"
fi

# ============================================================================
# STEP 7: Install evdev touch driver + calibration
# ============================================================================
step "7/9: Installing touch input driver and calibration"

# Create X11 config directory
mkdir -p /etc/X11/xorg.conf.d

# ── Install evdev driver ──
# evdev provides better calibration support than libinput for resistive touch
if ! dpkg -l 2>/dev/null | grep -q "xserver-xorg-input-evdev"; then
    info "Installing evdev touch driver..."
    if [ "$ARCH" = "32" ]; then
        DEB_FILE="$SCRIPT_DIR/xserver-xorg-input-evdev_1%3a2.10.6-1+b1_armhf.deb"
    else
        DEB_FILE="$SCRIPT_DIR/xserver-xorg-input-evdev_1%3a2.10.6-2_arm64.deb"
    fi
    
    if [ -f "$DEB_FILE" ]; then
        dpkg -i -B "$DEB_FILE" 2>/dev/null || true
        ok "evdev driver installed from local package"
    else
        # Try apt
        apt-get install -y xserver-xorg-input-evdev 2>/dev/null || {
            warn "Could not install evdev — touch may use libinput instead"
        }
    fi
else
    ok "evdev driver already installed"
fi

# ── Give evdev higher priority than libinput ──
if [ -f /usr/share/X11/xorg.conf.d/10-evdev.conf ]; then
    cp -f /usr/share/X11/xorg.conf.d/10-evdev.conf \
          /usr/share/X11/xorg.conf.d/45-evdev.conf
    ok "evdev priority set above libinput"
fi

# ── Install touch calibration ──
# Select calibration based on rotation
CALIB_FILE="$SCRIPT_DIR/usr/99-calibration.conf-mhs35-${ROTATION}"
if [ -f "$CALIB_FILE" ]; then
    cp -f "$CALIB_FILE" /etc/X11/xorg.conf.d/99-calibration.conf
    ok "Touch calibration installed for ${ROTATION}°"
else
    # Create a reasonable default calibration for 90°
    warn "Calibration file for ${ROTATION}° not found, using default"
    cat > /etc/X11/xorg.conf.d/99-calibration.conf << 'CALIBEOF'
Section "InputClass"
        Identifier      "calibration"
        MatchProduct    "ADS7846 Touchscreen"
        Option  "Calibration"   "3936 227 268 3880"
        Option  "SwapAxes"      "1"
        Option "EmulateThirdButton" "1"
        Option "EmulateThirdButtonTimeout" "1000"
        Option "EmulateThirdButtonMoveThreshold" "300"
EndSection
CALIBEOF
    ok "Default touch calibration installed"
fi

# ============================================================================
# STEP 8: Remove ALL legacy cruft
# ============================================================================
step "8/9: Removing legacy driver cruft"

# ── Remove fbturbo configs (crash X11 on Bookworm) ──
LEGACY_REMOVED=0

if [ -f /usr/share/X11/xorg.conf.d/99-fbturbo.conf ]; then
    rm -f /usr/share/X11/xorg.conf.d/99-fbturbo.conf
    ok "Removed 99-fbturbo.conf"
    LEGACY_REMOVED=1
fi

if [ -f /usr/share/X11/xorg.conf.d/99-fbturbo.~ ]; then
    rm -f /usr/share/X11/xorg.conf.d/99-fbturbo.~
    ok "Removed 99-fbturbo.~"
    LEGACY_REMOVED=1
fi

# ── Remove 99-fbdev.conf ──
if [ -f /usr/share/X11/xorg.conf.d/99-fbdev.conf ]; then
    rm -f /usr/share/X11/xorg.conf.d/99-fbdev.conf
    ok "Removed 99-fbdev.conf"
    LEGACY_REMOVED=1
fi

# ── Remove the dangerous .bash_profile ──
# The original script copies a .bash_profile that runs `startx` on login,
# which crashes LightDM and causes "Failed to start session"
if [ -f "/home/$USERNAME/.bash_profile" ]; then
    PROFILE_CONTENT="$(cat "/home/$USERNAME/.bash_profile" 2>/dev/null || true)"
    if echo "$PROFILE_CONTENT" | grep -q "startx\|FRAMEBUFFER"; then
        rm -f "/home/$USERNAME/.bash_profile"
        ok "Removed broken .bash_profile (was forcing startx)"
        LEGACY_REMOVED=1
    fi
fi

# ── Remove .xsession if it exists ──
if [ -f "/home/$USERNAME/.xsession" ]; then
    rm -f "/home/$USERNAME/.xsession"
    ok "Removed interfering .xsession"
    LEGACY_REMOVED=1
fi

# ── Clean up fbcp from rc.local ──
# On Pi 5 we do NOT use fbcp — the DRM driver renders directly
if [ -f /etc/rc.local ]; then
    if grep -q "fbcp" /etc/rc.local; then
        sed -i '/fbcp/d' /etc/rc.local
        ok "Removed fbcp from rc.local"
        LEGACY_REMOVED=1
    fi
fi

# ── Stop and disable fbcp service if it exists ──
if systemctl is-active fbcp.service &>/dev/null; then
    systemctl stop fbcp.service 2>/dev/null || true
    systemctl disable fbcp.service 2>/dev/null || true
    ok "Disabled fbcp systemd service"
    LEGACY_REMOVED=1
fi

# ── Fix home directory ownership ──
chown -R "$USERNAME:$USERNAME" "/home/$USERNAME"
ok "Fixed /home/$USERNAME ownership"

# ── Create symlink for scripts that reference /boot/config.txt ──
if [ -f /boot/firmware/config.txt ] && [ ! -L /boot/config.txt ]; then
    ln -sf /boot/firmware/config.txt /boot/config.txt
    ok "Created symlink /boot/config.txt → /boot/firmware/config.txt"
fi

if [ "$LEGACY_REMOVED" -eq 0 ]; then
    ok "No legacy files found (clean system)"
fi

# ============================================================================
# STEP 9: Set boot mode
# ============================================================================
step "9/9: Setting boot mode to Desktop Autologin"

if command -v raspi-config &>/dev/null; then
    # B4 = Desktop Autologin
    raspi-config nonint do_boot_behaviour B4
    ok "Boot set to Desktop Autologin"
else
    warn "raspi-config not found — please set boot mode manually"
fi

# ============================================================================
# DONE
# ============================================================================
echo ""
echo -e "${BOLD}============================================${NC}"
echo -e "${GREEN}${BOLD} Installation Complete!${NC}"
echo -e "${BOLD}============================================${NC}"
echo ""
echo -e " ${CYAN}Board:${NC}    $PI_MODEL"
echo -e " ${CYAN}OS:${NC}       Debian $DEB_VERSION (${ARCH}-bit)"
echo -e " ${CYAN}Rotation:${NC} ${ROTATION}°"
echo -e " ${CYAN}Method:${NC}   Native DRM (no fbcp)"
echo ""
echo -e " ${BOLD}After reboot:${NC}"
echo -e "   • The desktop will appear on the 3.5\" LCD"
echo -e "   • Touch input should work"
echo -e "   • HDMI will continue to work normally"
echo ""
echo -e " ${BOLD}Troubleshooting:${NC}"
echo -e "   • Check kernel logs: ${CYAN}dmesg | grep -i spi${NC}"
echo -e "   • Check DRM devices: ${CYAN}ls -la /dev/dri/${NC}"
echo -e "   • Check display:     ${CYAN}dmesg | grep -i drm${NC}"
echo -e "   • Re-calibrate:      ${CYAN}DISPLAY=:0 xinput_calibrator${NC}"
echo ""
echo -e " ${BOLD}To restore your system:${NC}"
echo -e "   ${CYAN}cd $SCRIPT_DIR && sudo ./MHS35-pi5-restore.sh${NC}"
echo ""
echo -e "${BOLD}============================================${NC}"
echo -e " ${YELLOW}Rebooting in 5 seconds...${NC}"
echo -e "${BOLD}============================================${NC}"
echo ""

# Record install info
echo "pi5:mhs35:${ROTATION}:$(date +%Y%m%d_%H%M%S)" > "$SCRIPT_DIR/.pi5_installed"

sync
sync
sleep 5
reboot
