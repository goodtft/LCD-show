# MHS35 3.5" SPI LCD — Raspberry Pi 4 Setup Guide

Quick reference for getting the MHS35 / MPI3501 3.5" SPI touch LCD working on a
**Raspberry Pi 4**, on **Raspberry Pi OS Bookworm (32-bit)**.

This uses `MHS35-fixed-install.sh`, which drives the display through the native
kernel `piscreen,drm` overlay (no `fbcp`). Confirmed working on this hardware.

---

## 1. Copy the repo to the Pi

From your PC (PowerShell / Git Bash), replace the user/IP with your Pi's:

```bash
scp -r "C:\Users\faisa\Desktop\dis\LCD-show" fai@10.76.184.22:/home/fai/
```

## 2. SSH into the Pi

```bash
ssh fai@10.76.184.22
```

## 3. Update firmware/kernel first

The `,drm` overlay flag needs a reasonably current kernel. Do this before
installing, especially on an OS image that hasn't been updated in a while:

```bash
sudo apt update && sudo apt full-upgrade -y && sudo reboot
```

Wait for the Pi to come back up, then SSH in again.

## 4. Run the installer

```bash
cd /home/fai/LCD-show
chmod +x MHS35-fixed-install.sh MHS35-fixed-restore.sh
sudo ./MHS35-fixed-install.sh 90
```

- Rotation argument: `0`, `90`, `180`, or `270` (`90` = landscape).
- The script backs up `config.txt`/`cmdline.txt`, installs the overlay,
  disables `vc4-kms-v3d` (the LCD becomes the primary display — HDMI may
  show low-res or nothing after this), sets up touch input (evdev +
  calibration), and reboots automatically after ~5 seconds.

## 5. To undo everything

```bash
cd /home/fai/LCD-show
sudo ./MHS35-fixed-restore.sh
```

---

## If the screen comes up white

1. **Check the script actually ran to completion** — rerun it and watch for
   a red `[FAIL]` line. A common cause is running it from the wrong
   directory, or an incomplete `scp` transfer (missing the `usr/` folder):

   ```bash
   ls -la usr/mhs35-overlay.dtb
   ```

   If that file is missing, the `scp -r` didn't copy the subfolder — redo
   step 1.

2. **Confirm the overlay actually landed in config.txt** — after a
   successful run you should see `piscreen` and a *commented-out*
   `vc4-kms-v3d` line:

   ```bash
   cat /boot/firmware/config.txt | grep -E "piscreen|vc4|dtparam=spi"
   ```

   Expected:
   ```
   #dtoverlay=vc4-kms-v3d
   dtparam=spi=on
   dtoverlay=piscreen,speed=18000000,drm
   ```

   If `vc4-kms-v3d` is still uncommented and there's no `piscreen` line,
   the install script didn't finish — go back to step 1.

3. **Kernel/driver-level check**:

   ```bash
   dmesg | grep -iE "spi|ili9486|piscreen|drm|fbtft"
   ls /dev/dri/ /dev/fb*
   vcgencmd version
   ```

---

## Notes

- `MHS35-fixed-install.sh` (Pi 4) and `MHS35-pi5-install.sh` (Pi 5) are
  separate scripts sharing the same approach — use the one matching your
  board.
- Changes to `config.txt` are cumulative-safe: rerunning the installer
  strips any previous MHS35-related lines before adding fresh ones, so you
  don't need to restore first between attempts.
