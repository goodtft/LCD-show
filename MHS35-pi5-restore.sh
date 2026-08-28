#!/bin/bash
# ============================================================================
# MHS35 3.5" SPI TFT LCD Display — Raspberry Pi 5 Restore Script
# ============================================================================
#
# Restores your Raspberry Pi 5 to its original state before the LCD was
# installed. Undoes all changes made by MHS35-pi5-install.sh.
#
# USAGE:
#   sudo ./MHS35-pi5-restore.sh
#
# ============================================================================

set -euo pipefail

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

if [ "$(id -u)" -ne 0 ]; then
    fail "This script must be run as root. Use: sudo $0"
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo ""
echo -e "${BOLD}============================================${NC}"
echo -e "${BOLD} MHS35 Display — Pi 5 System Restore${NC}"
echo -e "${BOLD}============================================${NC}"
echo ""

# Determine config paths
if [ -f /boot/firmware/config.txt ]; then
    CONFIG_PATH="/boot/firmware/config.txt"
    OVERLAY_DIR="/boot/firmware/overlays"
    CMDLINE_PATH="/boot/firmware/cmdline.txt"
elif [ -f /boot/config.txt ]; then
    CONFIG_PATH="/boot/config.txt"
    OVERLAY_DIR="/boot/overlays"
    CMDLINE_PATH="/boot/cmdline.txt"
else
    fail "Cannot find config.txt"
fi

# ── Try to find backup ──
BACKUP_DIR=""
if [ -f "$SCRIPT_DIR/.pi5_last_backup" ]; then
    BACKUP_DIR="$(cat "$SCRIPT_DIR/.pi5_last_backup")"
fi

# ── Restore config.txt ──
if [ -n "$BACKUP_DIR" ] && [ -f "$BACKUP_DIR/config.txt.bak" ]; then
    cp "$BACKUP_DIR/config.txt.bak" "$CONFIG_PATH"
    ok "Restored config.txt from backup"
else
    warn "No backup found — cleaning config.txt manually"
    
    # Remove our added block
    sed -i '/# --- MHS35 Pi5 LCD/,/# --- End MHS35 Pi5 LCD ---/d' "$CONFIG_PATH"
    
    # Remove individual LCD lines that might be left over
    sed -i '/^dtoverlay=mhs35/d'     "$CONFIG_PATH"
    sed -i '/^dtoverlay=piscreen/d'  "$CONFIG_PATH"
    sed -i '/^hdmi_force_hotplug/d'  "$CONFIG_PATH"
    sed -i '/^hdmi_group/d'          "$CONFIG_PATH"
    sed -i '/^hdmi_mode/d'           "$CONFIG_PATH"
    sed -i '/^hdmi_cvt/d'            "$CONFIG_PATH"
    sed -i '/^hdmi_drive/d'          "$CONFIG_PATH"
    
    # Re-enable KMS (in case it was disabled by another script)
    sed -i 's/^#dtoverlay=vc4-kms-v3d/dtoverlay=vc4-kms-v3d/' "$CONFIG_PATH"
    
    ok "config.txt cleaned"
fi

# ── Restore cmdline.txt ──
if [ -n "$BACKUP_DIR" ] && [ -f "$BACKUP_DIR/cmdline.txt.bak" ]; then
    cp "$BACKUP_DIR/cmdline.txt.bak" "$CMDLINE_PATH"
    ok "Restored cmdline.txt from backup"
elif [ -f "$CMDLINE_PATH" ]; then
    sed -i 's/ fbcon=map:[0-9]*//g' "$CMDLINE_PATH"
    ok "Cleaned cmdline.txt"
fi

# ── Restore rc.local ──
if [ -n "$BACKUP_DIR" ] && [ -f "$BACKUP_DIR/rc.local.bak" ]; then
    cp "$BACKUP_DIR/rc.local.bak" /etc/rc.local
    ok "Restored rc.local from backup"
else
    # Create a clean default
    cat > /etc/rc.local << 'EOF'
#!/bin/sh -e
_IP=$(hostname -I) || true
if [ "$_IP" ]; then
  printf "My IP address is %s\n" "$_IP"
fi
exit 0
EOF
    chmod +x /etc/rc.local
    ok "Created clean default rc.local"
fi

# ── Remove LCD overlay ──
rm -f "$OVERLAY_DIR/mhs35-overlay.dtb"
rm -f "$OVERLAY_DIR/mhs35.dtbo"
ok "Removed MHS35 overlay files"

# ── Remove touch calibration ──
rm -f /etc/X11/xorg.conf.d/99-calibration.conf
ok "Removed touch calibration"

# ── Clean up legacy files ──
rm -f /usr/share/X11/xorg.conf.d/99-fbturbo.conf
rm -f /usr/share/X11/xorg.conf.d/99-fbturbo.~
rm -f /usr/share/X11/xorg.conf.d/99-fbdev.conf
ok "Cleaned up X11 configs"

# ── Set boot to Desktop Autologin ──
if command -v raspi-config &>/dev/null; then
    raspi-config nonint do_boot_behaviour B4
    ok "Boot set to Desktop Autologin"
fi

# ── Remove install markers ──
rm -f "$SCRIPT_DIR/.pi5_installed"
rm -f "$SCRIPT_DIR/.pi5_last_backup"
rm -f "$SCRIPT_DIR/.have_installed"
ok "Removed install markers"

echo ""
echo -e "${BOLD}============================================${NC}"
echo -e "${GREEN}${BOLD} System Restored Successfully!${NC}"
echo -e "${BOLD}============================================${NC}"
echo -e " ${YELLOW}Rebooting in 3 seconds...${NC}"
echo -e "${BOLD}============================================${NC}"
echo ""

sync
sleep 3
reboot
