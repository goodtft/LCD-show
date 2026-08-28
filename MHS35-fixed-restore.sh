#!/bin/bash
# ============================================================================
# MHS35 3.5" TFT LCD Display - FIXED Restore Script (v2)
# ============================================================================
# Restores your Raspberry Pi to its original state (before the LCD was installed)
# Works with both v1 (fbcp) and v2 (DRM) installs
# ============================================================================

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BOLD='\033[1m'
NC='\033[0m'

ok()   { echo -e "  ${GREEN}[OK]${NC}  $1"; }
warn() { echo -e "  ${YELLOW}[WARN]${NC} $1"; }

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

BACKUP_DIR="$SCRIPT_DIR/.mhs35_backup"

echo ""
echo -e "${BOLD}============================================${NC}"
echo -e "${BOLD} MHS35 Display - System Restore${NC}"
echo -e "${BOLD}============================================${NC}"
echo ""

# Determine config path
if [ -f /boot/firmware/config.txt ]; then
    CONFIG_PATH="/boot/firmware/config.txt"
    OVERLAY_PATH="/boot/firmware/overlays"
    CMDLINE_PATH="/boot/firmware/cmdline.txt"
elif [ -f /boot/config.txt ]; then
    CONFIG_PATH="/boot/config.txt"
    OVERLAY_PATH="/boot/overlays"
    CMDLINE_PATH="/boot/cmdline.txt"
fi

# Restore config.txt from backup
if [ -f "$BACKUP_DIR/config.txt.backup" ]; then
    sudo cp "$BACKUP_DIR/config.txt.backup" "$CONFIG_PATH"
    ok "Restored config.txt from backup"
else
    warn "No config.txt backup found, cleaning manually..."
    # Remove v2 DRM install block
    sudo sed -i '/# --- MHS35 LCD/,/# --- End MHS35 LCD ---/d' "$CONFIG_PATH"
    # Remove v1 fbcp lines
    sudo sed -i '/^hdmi_force_hotplug/d' "$CONFIG_PATH"
    sudo sed -i '/^hdmi_group/d' "$CONFIG_PATH"
    sudo sed -i '/^hdmi_mode/d' "$CONFIG_PATH"
    sudo sed -i '/^hdmi_cvt/d' "$CONFIG_PATH"
    sudo sed -i '/^hdmi_drive/d' "$CONFIG_PATH"
    sudo sed -i '/^dtoverlay=mhs35/d' "$CONFIG_PATH"
    sudo sed -i '/^dtoverlay=piscreen/d' "$CONFIG_PATH"
    sudo sed -i '/^dtoverlay=tft35a/d' "$CONFIG_PATH"
    # Re-enable KMS driver
    sudo sed -i 's/^#dtoverlay=vc4-kms-v3d/dtoverlay=vc4-kms-v3d/' "$CONFIG_PATH"
    ok "Cleaned config.txt manually"
fi

# Restore cmdline.txt from backup
if [ -f "$BACKUP_DIR/cmdline.txt.backup" ] && [ -n "$CMDLINE_PATH" ]; then
    sudo cp "$BACKUP_DIR/cmdline.txt.backup" "$CMDLINE_PATH"
    ok "Restored cmdline.txt from backup"
else
    # Remove fbcon redirect if present
    if [ -n "$CMDLINE_PATH" ] && [ -f "$CMDLINE_PATH" ]; then
        sudo sed -i 's/ fbcon=map:[0-9]*//g' "$CMDLINE_PATH"
        ok "Cleaned cmdline.txt"
    fi
fi

# Restore rc.local
if [ -f "$BACKUP_DIR/rc.local.backup" ]; then
    sudo cp "$BACKUP_DIR/rc.local.backup" /etc/rc.local
    ok "Restored rc.local from backup"
else
    # Create a clean default rc.local (remove fbcp if present)
    sudo tee /etc/rc.local > /dev/null << 'EOF'
#!/bin/sh -e
_IP=$(hostname -I) || true
if [ "$_IP" ]; then
  printf "My IP address is %s\n" "$_IP"
fi
exit 0
EOF
    sudo chmod +x /etc/rc.local
    ok "Created clean rc.local"
fi

# Remove LCD overlay files
sudo rm -f "$OVERLAY_PATH/mhs35-overlay.dtb"
sudo rm -f "$OVERLAY_PATH/mhs35.dtbo"
ok "Removed LCD overlays"

# Remove touch calibration
sudo rm -f /etc/X11/xorg.conf.d/99-calibration.conf
ok "Removed touch calibration"

# Remove evdev priority override
sudo rm -f /usr/share/X11/xorg.conf.d/45-evdev.conf
ok "Removed evdev priority config"

# Remove broken files from any version
sudo rm -f /usr/share/X11/xorg.conf.d/99-fbturbo.conf
sudo rm -f /usr/share/X11/xorg.conf.d/99-fbturbo.~
sudo rm -f /usr/share/X11/xorg.conf.d/99-fbdev.conf
ok "Cleaned up X11 configs"

# Set boot to Desktop Autologin
if command -v raspi-config >/dev/null 2>&1; then
    sudo raspi-config nonint do_boot_behaviour B4
    ok "Set boot to Desktop Autologin"
fi

echo ""
echo -e "${BOLD}============================================${NC}"
echo -e "${GREEN}${BOLD} System Restored Successfully!${NC}"
echo -e "${BOLD}============================================${NC}"
echo -e " ${YELLOW}Rebooting in 3 seconds...${NC}"
echo -e "${BOLD}============================================${NC}"

sudo sync
sleep 3
sudo reboot
