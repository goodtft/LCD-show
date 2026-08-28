# Raspberry Pi MHS35 LCD Display: Troubleshooting & Resolution Report

## The Core Problem

The manufacturer's installation script (`LCD-show`) was heavily outdated. It was designed for older operating systems (Debian 10 Buster / Debian 11 Bullseye). When you ran it on the modern **Debian 12 (Bookworm)**, the script forcefully installed deprecated graphics drivers (`fbturbo` and `fbdev`).

Because Debian 12 uses a completely new graphical architecture (Wayland/KMS), these old drivers caused a massive conflict:

* **The Symptom:** A permanent black screen with a blinking cursor.
* **The Cause:** The X11 Display Server was trying to use a "shadow framebuffer" to render the screen, but the kernel was blocking it because the driver was obsolete, causing a total graphical lockup.

---

## Step-by-Step Solutions

### 1. Stripping Out the Broken Legacy Drivers

* **The Fix:** We completely deleted the broken `99-fbturbo.conf` and `99-fbdev.conf` files from the `/usr/share/X11/xorg.conf.d/` directory.
* **Why it worked:** This stopped the system from trying to use the crashing display methods and handed graphical control back to the modern operating system.

### 2. Enabling Native Hardware Acceleration (DRM/KMS)

* **The Fix:** We modified the `/boot/firmware/config.txt` file. We removed the generic `tft35a` overlay and replaced it with `dtoverlay=piscreen,rotate=270`.
* **Why it worked:** The `piscreen` overlay is a modern, native DRM/KMS driver built directly into the Linux kernel for the ILI9486 display chip. This bypassed the need for third-party scripts entirely, giving the screen hardware acceleration and a permanent, stable 270-degree landscape rotation.

### 3. Fixing the "Squashed" Touch Input

* **The Problem:** The default touch driver (`libinput`) was squashing the touch coordinates, making it impossible to accurately calibrate the screen.
* **The Fix:** We forced the system to use the older, but highly precise `evdev` input driver specifically for the touchscreen digitizer.
* **Why it worked:** The `evdev` driver natively supports matrix calibration, allowing for raw, pixel-perfect mapping of the physical resistive touch layer to the pixels on the screen.

### 4. Overcoming Calibration "Mis-Click" Errors

* **The Problem:** Because we rotated the screen 270 degrees in the kernel, the physical touch sensor was completely upside down and backward compared to the image. When running the `xinput_calibrator` tool, touching the top-left crosshair registered as a click on the bottom-right. The calibrator thought you were missing the target and kept throwing a *"Mis-click detected"* error.
* **The Fix:** We bypassed the failing calibration tool and manually hardcoded the exact hardware matrix into `/usr/share/X11/xorg.conf.d/99-calibration.conf`.
* **Why it worked:** By manually injecting `Option "SwapAxes" "1"` alongside the raw hardware coordinates (`278 3951 3862 303`), we successfully untangled the inverted X and Y axes, resulting in flawless stylus tracking right to the edges of the screen.

### 5. Desktop Interaction Setup

* **The Fix:** We bypassed the missing system menus by copying shortcuts for `lxterminal` and the `matchbox-keyboard` directly to the visible desktop.
* **Why it worked:** This provided full, independent control of the Raspberry Pi entirely via the 3.5-inch touch screen, eliminating the need for a physical keyboard and preparing the system for a final Python GUI deployment.

---

### Conclusion

By abandoning the manufacturer's outdated scripts and manually configuring the Raspberry Pi using native Linux kernel overlays, we transformed a completely broken setup into a highly stable, hardware-accelerated, and perfectly calibrated display.
