#!/bin/bash
# ============================================================================
# MHS35 3.5" TFT LCD Display - FIXED Installer (v2 - Native DRM)
# ============================================================================
#
# This script configures the MHS35 / MPI3501 3.5" SPI TFT LCD display
# using the NATIVE KERNEL DRM DRIVER (piscreen overlay).
#
# WHY v2?
#   The v1 script used fbcp (DispmanX framebuffer copy) to mirror HDMI → SPI.
#   On Raspberry Pi OS Bookworm (Debian 12, kernel 6.x+), DispmanX is
#   deprecated. fbcp produces a WHITE screen on the LCD and BLACK on HDMI.
#
#   This v2 script uses dtoverlay=piscreen with the ,drm flag, which is a
#   KERNEL-BUILT-IN overlay for ILI9486 SPI displays. The kernel drives
#   pixels directly over SPI — no fbcp middleman needed.
#
# Compatible with:
#   • Raspberry Pi 4 (all RAM variants)
#   • Raspberry Pi 5 (all RAM variants)
#   • Raspberry Pi OS Bookworm 32-bit & 64-bit (Debian 12.x)
#   • Raspberry Pi OS Bullseye 32-bit & 64-bit (Debian 11.x)
#   • MHS35 / MPI3501 displays (ILI9486 controller, XPT2046/ADS7846 touch)
#
# USAGE:
#   cd LCD-show
#   chmod +x MHS35-fixed-install.sh
#   sudo ./MHS35-fixed-install.sh [rotation]
#
#   rotation = 0, 90, 180, 270  (default: 90 = landscape)
#
# TO RESTORE:
#   sudo ./MHS35-fixed-restore.sh
#
# ============================================================================

# Exit on error, but handle expected failures gracefully
set -e

# ── Colours ──────────────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

ok()   { echo -e "  ${GREEN}[OK]${NC}  $1"; }
info() { echo -e "  ${CYAN}[INFO]${NC} $1"; }
warn() { echo -e "  ${YELLOW}[WARN]${NC} $1"; }
fail() { echo -e "  ${RED}[FAIL]${NC} $1"; exit 1; }

echo ""
echo -e "${BOLD}============================================${NC}"
echo -e "${BOLD} MHS35 Display - Fixed Installer (v2 DRM)${NC}"
echo -e "${BOLD}============================================${NC}"
echo ""

# ── Parse rotation argument ─────────────────────────────────────────────────
ROTATION="${1:-90}"
case "$ROTATION" in
    0|90|180|270) ;;
    *) fail "Invalid rotation '$ROTATION'. Use 0, 90, 180, or 270." ;;
esac

# ── Detect system info ───────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

deb_version="$(cat /etc/debian_version 2>/dev/null | tr -d '\n' || echo 'unknown')"
username="$(logname 2>/dev/null || echo "${SUDO_USER:-$USER}")"
hardware_arch="$(getconf LONG_BIT)"

if [ -f /proc/device-tree/model ]; then
    PI_MODEL="$(tr -d '\0' < /proc/device-tree/model)"
else
    PI_MODEL="unknown"
fi

info "Board: $PI_MODEL"
info "Architecture: ${hardware_arch}-bit"
info "Debian version: $deb_version"
info "Username: $username"
info "Rotation: ${ROTATION}°"

# ── Determine the correct config.txt path ────────────────────────────────────
# Bookworm (Debian 12+) uses /boot/firmware/config.txt
# Older versions use /boot/config.txt
if [ -f /boot/firmware/config.txt ]; then
    CONFIG_PATH="/boot/firmware/config.txt"
    OVERLAY_PATH="/boot/firmware/overlays"
    CMDLINE_PATH="/boot/firmware/cmdline.txt"
elif [ -f /boot/config.txt ]; then
    CONFIG_PATH="/boot/config.txt"
    OVERLAY_PATH="/boot/overlays"
    CMDLINE_PATH="/boot/cmdline.txt"
else
    fail "Cannot find config.txt! Is this a Raspberry Pi?"
fi

info "Config: $CONFIG_PATH"
info "Overlays: $OVERLAY_PATH"
echo ""

# ============================================================================
# STEP 1: Backup current config
# ============================================================================
echo -e "${BOLD}── STEP 1/8: Backing up current configuration ──${NC}"

BACKUP_DIR="$SCRIPT_DIR/.mhs35_backup"
sudo mkdir -p "$BACKUP_DIR"
sudo cp "$CONFIG_PATH" "$BACKUP_DIR/config.txt.backup"
if [ -f "$CMDLINE_PATH" ]; then
    sudo cp "$CMDLINE_PATH" "$BACKUP_DIR/cmdline.txt.backup"
fi
if [ -f /etc/rc.local ]; then
    sudo cp /etc/rc.local "$BACKUP_DIR/rc.local.backup"
fi

ok "Backup saved to $BACKUP_DIR"

# ============================================================================
# STEP 2: Switch display server to X11
# ============================================================================
echo -e "${BOLD}── STEP 2/8: Switching display server to X11 ──${NC}"

# SPI LCD displays require X11 — Wayland cannot render to SPI framebuffers
if command -v raspi-config >/dev/null 2>&1; then
    sudo raspi-config nonint do_wayland W1 2>/dev/null || true
    ok "Display server set to X11"
else
    warn "raspi-config not found — please manually switch to X11"
fi

# ============================================================================
# STEP 3: Enable SPI and install MHS35 device tree overlay
# ============================================================================
echo -e "${BOLD}── STEP 3/8: Enabling SPI and installing overlay ──${NC}"

# Enable SPI via raspi-config
if command -v raspi-config >/dev/null 2>&1; then
    sudo raspi-config nonint do_spi 0 2>/dev/null || true
fi

# Install the MHS35 overlay file
if [ -f ./usr/mhs35-overlay.dtb ]; then
    sudo cp ./usr/mhs35-overlay.dtb "$OVERLAY_PATH/"
    sudo cp ./usr/mhs35-overlay.dtb "$OVERLAY_PATH/mhs35.dtbo"
    ok "MHS35 overlay installed to $OVERLAY_PATH"
else
    fail "Cannot find usr/mhs35-overlay.dtb! Are you in the LCD-show directory?"
fi

# ============================================================================
# STEP 4: Configure config.txt for NATIVE DRM (THE CRITICAL FIX)
# ============================================================================
echo -e "${BOLD}── STEP 4/8: Configuring $CONFIG_PATH for native DRM ──${NC}"

# ── Remove ALL old LCD-related lines to start clean ──
sudo sed -i '/^dtoverlay=mhs35/d'           "$CONFIG_PATH"
sudo sed -i '/^dtoverlay=tft35a/d'          "$CONFIG_PATH"
sudo sed -i '/^dtoverlay=piscreen/d'        "$CONFIG_PATH"
sudo sed -i '/^hdmi_force_hotplug/d'        "$CONFIG_PATH"
sudo sed -i '/^hdmi_group/d'               "$CONFIG_PATH"
sudo sed -i '/^hdmi_mode/d'                "$CONFIG_PATH"
sudo sed -i '/^hdmi_cvt/d'                 "$CONFIG_PATH"
sudo sed -i '/^hdmi_drive/d'               "$CONFIG_PATH"
sudo sed -i '/^display_rotate/d'           "$CONFIG_PATH"
sudo sed -i '/^lcd_rotate/d'               "$CONFIG_PATH"
sudo sed -i '/^dtoverlay=ads7846/d'        "$CONFIG_PATH"
sudo sed -i '/^dtparam=speed=/d'           "$CONFIG_PATH"
# Remove any previous install block markers
sudo sed -i '/# --- MHS35 LCD/,/# --- End MHS35 LCD ---/d' "$CONFIG_PATH"

# ── CRITICAL: Disable vc4-kms-v3d ──
# WHY: On Bookworm, vc4-kms-v3d makes the GPU grab /dev/dri/card0 and renders
# ONLY to HDMI. The SPI LCD never receives any pixel data → WHITE SCREEN.
#
# By disabling vc4-kms-v3d and using piscreen with the ,drm flag, the SPI LCD
# becomes the primary (and only) DRM display. The kernel drives pixels
# directly over SPI to the ILI9486 controller.
#
# NOTE: HDMI output will show low resolution or may not display the desktop.
# This is EXPECTED — the LCD is the primary display.
sudo sed -i 's/^dtoverlay=vc4-kms-v3d/#dtoverlay=vc4-kms-v3d/' "$CONFIG_PATH"
sudo sed -i 's/^dtoverlay=vc4-fkms-v3d/#dtoverlay=vc4-fkms-v3d/' "$CONFIG_PATH"

info "Disabled vc4-kms-v3d (required for SPI LCD as primary display)"

# ── Ensure SPI and I2C are enabled ──
sudo sed -i 's/^#dtparam=spi=on/dtparam=spi=on/' "$CONFIG_PATH"
sudo sed -i 's/^#dtparam=i2c_arm=on/dtparam=i2c_arm=on/' "$CONFIG_PATH"
grep -q "^dtparam=spi=on" "$CONFIG_PATH" || echo "dtparam=spi=on" | sudo tee -a "$CONFIG_PATH" > /dev/null
grep -q "^dtparam=i2c_arm=on" "$CONFIG_PATH" || echo "dtparam=i2c_arm=on" | sudo tee -a "$CONFIG_PATH" > /dev/null

# ── Add native DRM LCD configuration ──
# piscreen is a KERNEL-BUILT-IN overlay for ILI9486 SPI displays.
# The ,drm flag forces it to use the modern DRM/TinyDRM subsystem instead
# of the legacy fbtft framebuffer — this is what makes it work on Bookworm.
# speed=18000000 (18 MHz) is safe for most MHS35/MPI3501 displays.
#
# NOTE: We always APPEND a fresh [all] block at the end of the file rather
# than inserting after an existing "[all]" line. Stock Raspberry Pi OS
# config.txt contains [all] TWICE (once near the top, once as a reset after
# [cm4]) — `sed '/^\[all\]/a ...'` fires on every match, so the old
# insert-after-[all] approach silently wrote the piscreen overlay into the
# file twice, causing a GPIO/pin-conflict error on the second application.
# Appending at the very end is always safe: [all] simply resets scope back
# to "applies to every board", so it works no matter what came before it.
{
    echo ""
    echo "[all]"
    echo "# --- MHS35 LCD (added by MHS35-fixed-install.sh v2) ---"
    echo "dtoverlay=piscreen,speed=18000000,drm"
    echo "hdmi_force_hotplug=1"
    echo "# --- End MHS35 LCD ---"
} | sudo tee -a "$CONFIG_PATH" > /dev/null

ok "config.txt configured for native DRM (piscreen + drm)"

# ============================================================================
# STEP 5: Configure console framebuffer redirect
# ============================================================================
echo -e "${BOLD}── STEP 5/8: Configuring framebuffer console ──${NC}"

# The SPI LCD appears as a secondary DRM device (usually /dev/fb1).
# Add fbcon=map:11 to cmdline.txt to redirect Linux console to it.
if [ -f "$CMDLINE_PATH" ]; then
    # Remove any existing fbcon= entries first
    sudo sed -i 's/ fbcon=map:[0-9]*//g' "$CMDLINE_PATH"
    # Add fbcon=map:11 (maps console to fb1, the SPI LCD)
    sudo sed -i 's/$/ fbcon=map:11/' "$CMDLINE_PATH"
    ok "Console framebuffer redirect configured"
else
    warn "cmdline.txt not found — console redirect skipped"
fi

# ============================================================================
# STEP 6: Configure touch input (evdev driver)
# ============================================================================
echo -e "${BOLD}── STEP 6/8: Configuring touch input ──${NC}"

# Create X11 config directory
sudo mkdir -p /etc/X11/xorg.conf.d

# ── Install evdev driver ──
# evdev provides better calibration support than libinput for resistive touch
if ! dpkg -s xserver-xorg-input-evdev >/dev/null 2>&1; then
    info "Installing evdev touch driver..."
    if [ "$hardware_arch" = "32" ]; then
        DEB_FILE="$SCRIPT_DIR/xserver-xorg-input-evdev_1%3a2.10.6-1+b1_armhf.deb"
    else
        DEB_FILE="$SCRIPT_DIR/xserver-xorg-input-evdev_1%3a2.10.6-2_arm64.deb"
    fi

    if [ -f "$DEB_FILE" ]; then
        sudo dpkg -i -B "$DEB_FILE" 2>/dev/null || true
        ok "evdev driver installed from local package"
    else
        # Try apt as fallback
        sudo apt-get install -y xserver-xorg-input-evdev 2>/dev/null || {
            warn "Could not install evdev — touch may use libinput instead"
        }
    fi
else
    ok "evdev driver already installed"
fi

# ── Give evdev higher priority than libinput ──
if [ -f /usr/share/X11/xorg.conf.d/10-evdev.conf ]; then
    sudo cp -rf /usr/share/X11/xorg.conf.d/10-evdev.conf /usr/share/X11/xorg.conf.d/45-evdev.conf
    ok "evdev priority set above libinput"
fi

# ── Install touch calibration ──
CALIB_FILE="$SCRIPT_DIR/usr/99-calibration.conf-mhs35-${ROTATION}"
if [ -f "$CALIB_FILE" ]; then
    sudo cp -rf "$CALIB_FILE" /etc/X11/xorg.conf.d/99-calibration.conf
    ok "Touch calibration installed for ${ROTATION}°"
else
    # Create a reasonable default calibration for 90°
    warn "Calibration file for ${ROTATION}° not found, using default"
    sudo tee /etc/X11/xorg.conf.d/99-calibration.conf > /dev/null << 'CALIBEOF'
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

ok "Touch input configured"

# ============================================================================
# STEP 7: Remove ALL legacy cruft that crashes the desktop
# ============================================================================
echo -e "${BOLD}── STEP 7/8: Removing legacy driver cruft ──${NC}"

LEGACY_REMOVED=0

# ── Remove fbturbo configs (crash X11 on Bookworm) ──
if [ -f /usr/share/X11/xorg.conf.d/99-fbturbo.conf ]; then
    sudo rm -f /usr/share/X11/xorg.conf.d/99-fbturbo.conf
    ok "Removed 99-fbturbo.conf"
    LEGACY_REMOVED=1
fi
if [ -f /usr/share/X11/xorg.conf.d/99-fbturbo.~ ]; then
    sudo rm -f /usr/share/X11/xorg.conf.d/99-fbturbo.~
    ok "Removed 99-fbturbo.~"
    LEGACY_REMOVED=1
fi

# ── Remove 99-fbdev.conf ──
if [ -f /usr/share/X11/xorg.conf.d/99-fbdev.conf ]; then
    sudo rm -f /usr/share/X11/xorg.conf.d/99-fbdev.conf
    ok "Removed 99-fbdev.conf"
    LEGACY_REMOVED=1
fi

# ── Remove the dangerous .bash_profile ──
# The original script copies a .bash_profile that runs `startx` on login,
# which crashes LightDM and causes "Failed to start session"
if [ -f "/home/$username/.bash_profile" ]; then
    PROFILE_CONTENT="$(cat "/home/$username/.bash_profile" 2>/dev/null || true)"
    if echo "$PROFILE_CONTENT" | grep -q "startx\|FRAMEBUFFER"; then
        rm -f "/home/$username/.bash_profile"
        ok "Removed broken .bash_profile (was forcing startx)"
        LEGACY_REMOVED=1
    fi
fi

# ── Remove .xsession if it exists ──
if [ -f "/home/$username/.xsession" ]; then
    rm -f "/home/$username/.xsession"
    ok "Removed interfering .xsession"
    LEGACY_REMOVED=1
fi

# ── Clean up fbcp from rc.local ──
# With native DRM we do NOT use fbcp — the DRM driver renders directly
if [ -f /etc/rc.local ]; then
    if grep -q "fbcp" /etc/rc.local; then
        sudo sed -i '/fbcp/d' /etc/rc.local
        ok "Removed fbcp from rc.local (not needed with DRM)"
        LEGACY_REMOVED=1
    fi
fi

# ── Stop and disable fbcp service if it exists ──
if systemctl is-active fbcp.service >/dev/null 2>&1; then
    sudo systemctl stop fbcp.service 2>/dev/null || true
    sudo systemctl disable fbcp.service 2>/dev/null || true
    ok "Disabled fbcp systemd service"
    LEGACY_REMOVED=1
fi

# ── Fix home directory ownership ──
sudo chown -R "$username:$username" "/home/$username"
ok "Fixed /home/$username ownership"

# ── Create symlink for scripts that reference /boot/config.txt ──
if [ -f /boot/firmware/config.txt ] && [ ! -L /boot/config.txt ]; then
    sudo ln -sf /boot/firmware/config.txt /boot/config.txt
    ok "Created symlink /boot/config.txt → /boot/firmware/config.txt"
fi

if [ "$LEGACY_REMOVED" -eq 0 ]; then
    ok "No legacy files found (clean system)"
fi

# ============================================================================
# STEP 8: Set boot mode to Desktop Autologin
# ============================================================================
echo -e "${BOLD}── STEP 8/8: Setting boot mode ──${NC}"

if command -v raspi-config >/dev/null 2>&1; then
    # B4 = Desktop Autologin
    sudo raspi-config nonint do_boot_behaviour B4
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
echo -e " ${CYAN}OS:${NC}       Debian $deb_version (${hardware_arch}-bit)"
echo -e " ${CYAN}Rotation:${NC} ${ROTATION}°"
echo -e " ${CYAN}Method:${NC}   Native DRM (piscreen — no fbcp needed)"
echo ""
echo -e " ${BOLD}After reboot:${NC}"
echo -e "   • The desktop will appear on the 3.5\" LCD"
echo -e "   • Touch input should work"
echo -e "   • HDMI monitor may show low resolution (this is normal)"
echo ""
echo -e " ${BOLD}Troubleshooting:${NC}"
echo -e "   • Check kernel logs: ${CYAN}dmesg | grep -i spi${NC}"
echo -e "   • Check DRM devices: ${CYAN}ls -la /dev/dri/${NC}"
echo -e "   • Check display:     ${CYAN}dmesg | grep -i drm${NC}"
echo -e "   • Re-calibrate:      ${CYAN}DISPLAY=:0 xinput_calibrator${NC}"
echo ""
echo -e " ${BOLD}To restore your system:${NC}"
echo -e "   ${CYAN}cd $SCRIPT_DIR && sudo ./MHS35-fixed-restore.sh${NC}"
echo ""
echo -e "${BOLD}============================================${NC}"
echo -e " ${YELLOW}Rebooting in 5 seconds...${NC}"
echo -e "${BOLD}============================================${NC}"

sudo sync
sudo sync
sleep 5
sudo reboot
