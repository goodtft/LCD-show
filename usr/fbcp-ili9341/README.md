# Introduction

This repository implements a driver for certain SPI-based LCD displays for Raspberry Pi A, B, 2, 3, 4 and Zero.

![PiTFT display](/example.jpg "Adafruit PiTFT 2.8 with ILI9341 controller")

The work was motivated by curiosity after seeing this series of videos on the RetroManCave YouTube channel:
 - [RetroManCave: Waveshare 3.5" Raspberry Pi Screen | Review](https://www.youtube.com/watch?v=SGMC0t33C50)
 - [RetroManCave: Waveshare 3.2" vs 3.5" LCD screen gaming test | Raspberry Pi / RetroPie](https://www.youtube.com/watch?v=8bazEcXemiA)
 - [Elecrow 5 Inch LCD Review | RetroPie & Raspberry Pi](https://www.youtube.com/watch?v=8VgNBDMOssg)

In these videos, the SPI (GPIO) bus is referred to being the bottleneck. SPI based displays update over a serial data bus, transmitting one bit per clock cycle on the bus. A 320x240x16bpp display hence requires a SPI bus clock rate of 73.728MHz to achieve a full 60fps refresh frequency. Not many SPI LCD controllers can communicate this fast in practice, but are constrained to e.g. a 16-50MHz SPI bus clock speed, capping the maximum update rate significantly. Can we do anything about this?

The fbcp-ili9341 project started out as a display driver for the [Adafruit 2.8" 320x240 TFT w/ Touch screen for Raspberry Pi](https://www.adafruit.com/product/1601) display that utilizes the ILI9341 controller. On that display, fbcp-ili9341 can achieve a 60fps update rate, depending on the content that is being displayed. Check out these videos for examples of the driver in action:

 - [fbcp-ili9341 frame delivery smoothness test on Pi 3B and Adafruit ILI9341 at 119Hz](https://youtu.be/IqzKT33Rwjc)
 - [Latency and tearing test #2: GPIO input to display latency in fbcp-ili9341 and tearing modes](https://www.youtube.com/watch?v=EOICdpjiqv8)
 - [Latency and tearing test: KeDei 3.5" 320x480 HDMI vs Adafruit 2.8" PiTFT ILI9341 240x320 SPI](https://www.youtube.com/watch?v=1yvmvv0KtNs)
 - [fbcp-ili9341 ported to ILI9486 WaveShare 3.5" (B) SpotPear 320x480 SPI display](https://www.youtube.com/watch?v=dqOLIHOjLq4)
 - [Quake 60 fps inside Gameboy Advance (ILI9341)](https://www.youtube.com/watch?v=xmO8t3XlxVM)
 - First implementation of a statistics overlay: [fbcp-ili9341 SPI display driver on Adafruit PiTFT 2.8"](http://youtu.be/rKSH048XRjA)
 - Initial proof of concept video: [fbcp-ili9341 driver first demo](https://youtu.be/h1jhuR-oZm0)

### How It Works

Given that the SPI bus can be so constrained on bandwidth, how come fbcp-ili9341 seems to be able to update at up to 60fps? The way this is achieved is by what could be called *adaptive display stream updates*. Instead of uploading each pixel at each display refresh cycle, only the actually changed pixels on screen are submitted to the display. This is doable because the ILI9341 controller, as many other popular controllers, have communication interface functions that allow specifying partial screen updates, down to subrectangles or even individual pixel levels. This allows beating the bandwidth limit: for example in Quake, even though it is a fast pacing game, on average only about 46% of all pixels on screen change each rendered frame. Some parts, such as the UI stay practically constant across multiple frames.

Other optimizations are also utilized to squeeze out even more performance:
 - The program directly communicates with the BCM2835 ARM Peripherals controller registers, bypassing the usual Linux software stack.
 - A hybrid of both Polled Mode SPI and DMA based transfers are utilized. Long sequential transfer bursts are performed using DMA, and when DMA would have too much latency, Polled Mode SPI is applied instead.
 - Undocumented BCM2835 features are used to squeeze out maximum bandwidth: [SPI CDIV is driven at even numbers](https://www.raspberrypi.org/forums/viewtopic.php?t=43442) (and not just powers of two), and the [SPI DLEN register is forced in non-DMA mode](https://www.raspberrypi.org/forums/viewtopic.php?t=181154) to avoid an idle 9th clock cycle for each transferred byte.
 - Good old **interlacing** is added into the mix: if the amount of pixels that needs updating is detected to be too much that the SPI bus cannot handle it, the driver adaptively resorts to doing an interlaced update, uploading even and odd scanlines at subsequent frames. Once the number of pending pixels to write returns to manageable amounts, progressive updating is resumed. This effectively doubles the maximum display update rate. (If you do not like the visual appearance that interlacing causes, it is easy to disable this by uncommenting the line `#define NO_INTERLACING` in file `config.h`)
 - A dedicated SPI communication thread is used in order to keep the SPI bus active at all times.
 - A number of other micro-optimization techniques are used, such as batch updating rectangular spans of pixels, merging disjoint-but-close spans of pixels on the same scanline, and latching Column and Page End Addresses to bottom-right corner of the display to be able to cut CASET and PASET messages in mid-communication.

The result is that the SPI bus can be kept close to 100% saturation, ~94-97% usual, to maximize the utilization rate of the bus, while only transmitting practically the minimum number of bytes needed to describe each new frame.

### Tested Devices

The driver has been checked to work (at least some point in the past) on the following systems:

 - Raspberry Pi 3 Model B+ with Raspbian Stretch (GCC 6.3.0)
 - Raspberry Pi 3 Model B Rev 1.2 with Raspbian Jessie (GCC 4.9.2) and Raspbian Stretch (GCC 6.3.0)
 - Raspberry Pi Zero W with Raspbian Jessie (GCC 4.9.2) and Raspbian Stretch (GCC 6.3.0)
 - Raspberry Pi 2 Model B
 - Raspberry Pi B Rev. 2.0 (old board from Q4 2012, board revision ID 000e)

although not all boards are actively tested on, so ymmv especially on older boards. (Bug fixes welcome, use https://elinux.org/RPi_HardwareHistory to identify which board you are running on)

### Tested Displays

The following LCD displays have been tested:

 - [Adafruit 2.8" 320x240 TFT w/ Touch screen for Raspberry Pi](https://www.adafruit.com/product/1601) with ILI9341 controller
 - [Adafruit PiTFT 2.2" HAT Mini Kit - 320x240 2.2" TFT - No Touch](https://www.adafruit.com/product/2315) with ILI9340 controller
 - [Adafruit PiTFT - Assembled 480x320 3.5" TFT+Touchscreen for Raspberry Pi](https://www.adafruit.com/product/2097) with HX8357D controller
 - [Adafruit 128x96 OLED Breakout Board - 16-bit Color 1.27" w/microSD holder](https://www.adafruit.com/product/1673) with SSD1351 controller
 - [Waveshare 3.5inch RPi LCD (B) 320*480 Resolution Touch Screen IPS TFT Display](https://www.amazon.co.uk/dp/B01N48NOXI/ref=pe_3187911_185740111_TE_item) with ILI9486 controller
 - [maithoga 3.5 inch 8PIN SPI TFT LCD Color Screen with Adapter Board ILI9486](https://www.aliexpress.com/item/3-5-inch-8P-SPI-TFT-LCD-Color-Screen-Module-ILI9486-Drive-IC-320-480-RGB/32828284227.html) with **ILI9486L** controller
 - [BuyDisplay.com 320x480 Serial SPI 3.2"TFT LCD Module Display](https://www.buydisplay.com/default/serial-spi-3-2-inch-tft-lcd-module-display-ili9341-power-than-sainsmart) with ILI9341 controller
 - [Arduino A000096 1.77" 160x128 LCD Screen](https://store.arduino.cc/arduino-lcd-screen) with ST7735R controller
 - [Tontec 3.5" 320x480 LCD Display](https://www.ebay.com/p/Tontec-3-5-Inches-Touch-Screen-for-Raspberry-Pi-Display-TFT-Monitor-480x320-LCD/1649448059) with MZ61581-PI-EXT 2016.1.28 controller
 - [Adafruit 1.54" 240x240 Wide Angle TFT LCD Display with MicroSD](https://www.adafruit.com/product/3787) with ST7789 controller
 - [WaveShare 240x240, 1.3inch IPS LCD display HAT for Raspberry Pi](https://www.waveshare.com/1.3inch-lcd-hat.htm) with ST7789VW controller
 - [WaveShare 128x128, 1.44inch LCD display HAT for Raspberry Pi](https://www.waveshare.com/1.44inch-lcd-hat.htm) with ST7735S controller
 - [KeDei 3.5 inch SPI TFTLCD 480*320 16bit/18bit version 6.3 2018/4/9](https://github.com/juj/fbcp-ili9341/issues/40) with MPI3501 controller
 - Unbranded 2.8" 320x240 display with ILI9340 controller

### Installation

Check the following sections to set up the driver.

##### Boot configuration

This driver does not utilize the [notro/fbtft](https://github.com/notro/fbtft) framebuffer driver, so that needs to be disabled if active. That is, if your `/boot/config.txt` file has lines that look something like `dtoverlay=pitft28r, ...`, `dtoverlay=waveshare32b, ...` or `dtoverlay=flexfb, ...`, those should be removed.

This program neither utilizes the default SPI driver, so a line such as `dtparam=spi=on` in `/boot/config.txt` should also be removed so that it will not cause conflicts.

Likewise, if you have any touch controller related dtoverlays active, such as `dtoverlay=ads7846,...` or anything that has a `penirq=` directive, those should be removed as well to avoid conflicts. It would be possible to add touch support to fbcp-ili9341 if someone wants to take a stab at it.

##### Building and running

Run in the console of your Raspberry Pi:

```bash
sudo apt-get install cmake
cd ~
git clone https://github.com/juj/fbcp-ili9341.git
cd fbcp-ili9341
mkdir build
cd build
cmake [options] ..
make -j
sudo ./fbcp-ili9341
```

Note especially the two dots `..` on the CMake line, which denote "up one directory" in this case (instead of referring to "more items go here").

See the next section to see what to input under **[options]**.

If you have been running existing `fbcp` driver, make sure to remove that e.g. via a `sudo pkill fbcp` first (while running in SSH prompt or connected to a HDMI display), these two cannot run at the same time. If `/etc/rc.local` or `/etc/init.d` contains an entry to start up `fbcp` at boot, that directive should be deleted.

##### Configuring build options

There are generally two ways to configure build options, at CMake command line, and in the file [config.h](https://github.com/juj/fbcp-ili9341/blob/master/config.h).

On the CMake command line, the following options can be configured:

###### If you have a display Add-On Hat

When using one of the displays that stack on top of the Pi that are already recognized by fbcp-ili9341, you don't need to specify the GPIO pin assignments, but fbcp-ili9341 code already has those. Pass one of the following CMake directives for the hats:

- `-DADAFRUIT_ILI9341_PITFT=ON`: If you are running on the [Adafruit 2.8" 320x240 TFT w/ Touch screen for Raspberry Pi](https://www.adafruit.com/product/1601) (or the [Adafruit PiTFT 2.2" HAT Mini Kit - 320x240 2.2" TFT - No Touch](https://www.adafruit.com/product/2315) display, which is compatible), pass this flag.
- `-DADAFRUIT_HX8357D_PITFT=ON`: If you have the [Adafruit PiTFT - Assembled 480x320 3.5" TFT+Touchscreen for Raspberry Pi](https://www.adafruit.com/product/2097) display, add this line.
- `-DFREEPLAYTECH_WAVESHARE32B=ON`: If you are running on the [Freeplay CM3 or Zero](https://www.freeplaytech.com/product/freeplay-cm3-diy-kit/) device, pass this flag. (this is not a hat, but still a preconfigured pin assignment)
- `-DWAVESHARE35B_ILI9486=ON`: If specified, targets a [Waveshare 3.5" 480x320 ILI9486](https://www.amazon.co.uk/dp/B01N48NOXI/ref=pe_3187911_185740111_TE_item) display.
- `-DTONTEC_MZ61581=ON`: If you are running on the [Tontec 3.5" 320x480 LCD Display](https://www.ebay.com/p/Tontec-3-5-Inches-Touch-Screen-for-Raspberry-Pi-Display-TFT-Monitor-480x320-LCD/1649448059) display, pass this.
- `-DWAVESHARE_ST7789VW_HAT=ON`: If specified, targets a [240x240, 1.3inch IPS LCD display HAT for Raspberry Pi](https://www.waveshare.com/1.3inch-lcd-hat.htm) with ST7789VW display controller.
- `-DWAVESHARE_ST7735S_HAT=ON`: If specified, targets a [128x128, 1.44inch LCD display HAT for Raspberry Pi](https://www.waveshare.com/1.3inch-lcd-hat.htm) with ST7735S display controller.
- `-DKEDEI_V63_MPI3501=ON`: If specified, targets a [KeDei 3.5 inch SPI TFTLCD 480*320 16bit/18bit version 6.3 2018/4/9](https://github.com/juj/fbcp-ili9341/issues/40) display with MPI3501 display controller.

###### If you wired the display to the Pi yourself

If you connected wires directly on the Pi instead of using a Hat from the above list, you will need to use the configuration directives below. In addition to specifying the display, you will also need to tell fbcp-ili9341 which GPIO pins you wired the connections to. To configure the display controller, pass one of:

- `-DILI9341=ON`: If you are running on any other generic ILI9341 display, or on Waveshare32b display that is standalone and not on the FreeplayTech CM3/Zero device, pass this flag.
- `-DILI9340=ON`: If you have a ILI9340 display, pass this directive. ILI9340 and ILI9341 chipsets are very similar, but ILI9340 doesn't support all of the features on ILI9341 and they will be disabled or downgraded.
- `-DHX8357D=ON`: If you have a HX8357D display, pass this directive.
- `-DSSD1351=ON`: If you have a SSD1351 OLED display, use this.
- `-DST7735R=ON`: If you have a ST7735R display, use this.
- `-DST7789=ON`: If you have a ST7789 display, use this.
- `-DST7789VW=ON`: If you have a ST7789VW display, use this.
- `-DST7735S=ON`: If you have a ST7735S display, use this.
- `-DILI9486=ON`: If you have a ILI9486 display, pass this directive.
- `-DILI9486L=ON`: If you have a ILI9486L display, pass this directive. Note that ILI9486 and ILI9486L are quite different, mutually incompatible controller chips, so be careful here identifying which one you have. (or just try both, should not break if you misidentified)
- `-DILI9488=ON`: If you have a ILI9488 display, pass this directive.
- `-DMPI3501=ON`: If specified, targets a display with MPI3501 display controller.

And additionally, pass the following to customize the GPIO pin assignments you used:

- `-DGPIO_TFT_DATA_CONTROL=number`: Specifies/overrides which GPIO pin to use for the Data/Control (DC) line on the 4-wire SPI communication. This pin number is specified in BCM pin numbers. If you have a 3-wire SPI display that does not have a Data/Control line, **set this value to -1**, i.e. `-DGPIO_TFT_DATA_CONTROL=-1` to tell fbcp-ili9341 to target 3-wire ("9-bit") SPI communication.
- `-DGPIO_TFT_RESET_PIN=number`: Specifies/overrides which GPIO pin to use for the display Reset line. This pin number is specified in BCM pin numbers. If omitted, it is assumed that the display does not have a Reset pin, and is always on.
- `-DGPIO_TFT_BACKLIGHT=number`: Specifies/overrides which GPIO pin to use for the display backlight line. This pin number is specified in BCM pin numbers. If omitted, it is assumed that the display does not have a GPIO-controlled backlight pin, and is always on. If setting this, also see the `#define BACKLIGHT_CONTROL` option in `config.h`.

fbcp-ili9341 always uses the hardware SPI0 port, so the MISO, MOSI, CLK and CE0 pins are always the same and cannot be changed. The MISO pin is actually not used (at the moment at least), so you can just skip connecting that one. If your display is a rogue one that ignores the chip enable line, you can omit connecting that as well, or might also be able to get away by connecting that to ground if you are hard pressed to simplify wiring (depending on the display).

###### Specifying display speed

To get good performance out of the displays, you will drive the displays far out above the rated speed specs (the rated specs yield about ~10fps depending on display). Due to this, you will need to explicitly configure the target speed you want to drive the display at, because due to manufacturing variances each display copy reaches a different maximum speed. There is no "default speed" that fbcp-ili9341 would use. Setting the speed is done via the option

- `-DSPI_BUS_CLOCK_DIVISOR=even_number`: Sets the clock divisor number which along with the Pi [core_freq=](https://www.raspberrypi.org/documentation/configuration/config-txt/overclocking.md) option in `/boot/config.txt` specifies the overall speed that the display SPI communication bus is driven at. `SPI_frequency = core_freq/divisor`. `SPI_BUS_CLOCK_DIVISOR` must be an even number. Default Pi 3B and Zero W `core_freq` is 400MHz, and generally a value `-DSPI_BUS_CLOCK_DIVISOR=6` seems to be the best that a ILI9341 display can do. Try a larger value if the display shows corrupt output, or a smaller value to get higher bandwidth. See [ili9341.h](https://github.com/juj/fbcp-ili9341/blob/master/ili9341.h#L13) and [waveshare35b.h](https://github.com/juj/fbcp-ili9341/blob/master/waveshare35b.h#L10) for data points on tuning the maximum SPI performance. Safe initial value could be something like `-DSPI_BUS_CLOCK_DIVISOR=30`.

###### Specifying the target Pi hardware

There are a couple of options to explicitly say which Pi board you want to target. These should be autodetected for you and generally are not needed, but e.g. if you are cross compiling for another Pi board from another system, or want to be explicit, you can try:

- `-DSINGLE_CORE_BOARD=ON`: Pass this option if you are running on a Pi that has only one hardware thread (Pi Model A, Pi Model B, Compute Module 1, Pi Zero/Zero W). If not present, autodetected.
- `-DARMV6Z=ON`: Pass this option to specifically optimize for ARMv6Z instruction set (Pi 1A, 1A+, 1B, 1B+, Zero, Zero W). If not present, autodetected.
- `-DARMV7A=ON`: Pass this option to specifically optimize for ARMv7-A instruction set (Pi 2B < rev 1.2). If not present, autodetected.
- `-DARMV8A=ON`: Pass this option to specifically optimize for ARMv8-A instruction set (Pi 2B >= rev. 1.2, 3B, 3B+, CM3, CM3 lite or 4B). If not present, autodetected.

###### Specifying other build options

The following build options are general to all displays and Pi boards, they further customize the build:

- `-DBACKLIGHT_CONTROL=ON`: If set, enables fbcp-ili9341 to control the display backlight in the given backlight pin. The display will go to sleep after a period of inactivity on the screen. If not, backlight is not touched.
- `-DDISPLAY_CROPPED_INSTEAD_OF_SCALING=ON`: If set, and source video frame is larger than the SPI display video resolution, the source video is presented on the SPI display by cropping out parts of it in all directions, instead of scaling to fit.
- `-DDISPLAY_BREAK_ASPECT_RATIO_WHEN_SCALING=ON`: When scaling source video to SPI display, scaling is performed by default following aspect ratio, adding letterboxes/pillarboxes as needed. If this is set, the stretching is performed breaking aspect ratio.
- `-DSTATISTICS=number`: Specifies the level of overlay statistics to show on screen. 0: disabled, 1: enabled, 2: enabled, and show frame rate interval graph as well. Default value is 1 (enabled).
- `-DUSE_DMA_TRANSFERS=OFF`: If specified, disables using DMA transfers (at great expense of lost CPU usage). Pass this directive if DMA is giving some issues, e.g. as a troubleshooting step if something is not looking right.
- `-DDMA_TX_CHANNEL=<num>`: Specifies the DMA channel number to use for SPI send commands. Change this if you find a DMA channel conflict.
- `-DDMA_RX_CHANNEL=<num>`: Specifies the DMA channel number to use for SPI receive commands. Change this if you find a DMA channel conflict.
- `-DDISPLAY_SWAP_BGR=ON`: If this option is passed, red and blue color channels are reversed (RGB<->BGR) swap. Some displays have an opposite color panel subpixel layout that the display controller does not automatically account for, so define this if blue and red are mixed up.
- `-DDISPLAY_INVERT_COLORS=ON`: If this option is passed, pixel color value interpretation is reversed (white=0, black=31/63). Default: black=0, white=31/63. Pass this option if the display image looks like a color negative of the actual colors.
- `-DDISPLAY_ROTATE_180_DEGREES=ON`: If set, display is rotated 180 degrees. This does not affect HDMI output, only the SPI display output.
- `-DLOW_BATTERY_PIN=<num>`: Specifies a GPIO pin that can be polled to get the battery state. By default, when this is set, a low battery icon will be displayed if the pin is pulled low (see `config.h` for ways in which this can be tweaked).

In addition to the above CMake directives, there are various defines scattered around the codebase, mostly in [config.h](https://github.com/juj/fbcp-ili9341/blob/master/config.h), that control different runtime options. Edit those directly to further tune the behavior of the program. In particular, after you have finished with the setup, you may want to build with `-DSTATISTICS=0` option in CMake configuration line.

##### Build example

Here is a full example of what to type to build and run, if you have the [Adafruit 2.8" 320x240 TFT w/ Touch screen for Raspberry Pi](https://www.adafruit.com/product/1601) with ILI9341 controller:

```bash
cd ~
sudo apt-get install cmake
git clone https://github.com/juj/fbcp-ili9341.git
cd fbcp-ili9341
mkdir build
cd build
cmake -DSPI_BUS_CLOCK_DIVISOR=6 -DADAFRUIT_ILI9341_PITFT=ON ..
make -j
sudo ./fbcp-ili9341
```

If the above does not work, try specifying `-DSPI_BUS_CLOCK_DIVISOR=8` or `=10` to make the display run a little slower, or try with `-DUSE_DMA_TRANSFERS=OFF` to troubleshoot if DMA might be the issue. If you are using another display controller than ILI9341, using a much higher value, like 30 or 40 may be needed. When changing CMake options, you can reissue the CMake directive line without having to reclone or recreate the `build` directory. However you may need to manually delete file CMakeCache.txt between changing options to avoid CMake remembering old settings.

If you want to do a full rebuild from scratch, you can `rm -rf build` to delete the build directory and recreate it for a clean rebuild from scratch. There is nothing special about the name or location of this directory, it is just my usual convention. You can also do the build in some other directory relative to the fbcp-ili9341 directory if you please.

##### Launching the display driver at startup

To set up the driver to launch at startup, edit the file `/etc/rc.local` in `sudo` mode, and add a line

```bash
sudo /path/to/fbcp-ili9341/build/fbcp-ili9341 &
````

to the end. Make note of the needed ampersand `&` at the end of that line.

For example, if you used the command line steps listed above to build, the file `/etc/rc.local` would receive a line

```bash
sudo /home/pi/fbcp-ili9341/build/fbcp-ili9341 &
````

If the user name of your Raspberry Pi installation is something else than the default `pi`, change the directory accordingly to point to the user's home directory. (Use `pwd` to find out the current directory in terminal)

##### Configuring HDMI and TFT display sizes

If the size of the default HDMI output `/dev/fb0` framebuffer differs from the resolution of the display, the source video size will by default be rescaled to fit to the size of the SPI display. fbcp-ili9341 will manage setting up this rescaling if needed, and it will be done by the GPU, so performance should not be impacted too much. However if the resolutions do not match, small text will probably appear illegible. The resizing will be done in aspect ratio preserving manner, so if the aspect ratios do not match, either horizontal or vertical black borders will appear on the display. If you do not use the HDMI output at all, it is probably best to configure the HDMI output to match the SPI display size so that rescaling will not be needed. This can be done by setting the following lines in `/boot/config.txt`:

```
hdmi_group=2
hdmi_mode=87
hdmi_cvt=320 240 60 1 0 0 0
hdmi_force_hotplug=1
```

If your SPI display has a different resolution than 320x240, change the `320 240` part to e.g. `480 320`.

These lines hint native applications about the default display mode, and let them render to the native resolution of the TFT display. This can however prevent the use of the HDMI connector, if the HDMI connected display does not support such a small resolution. As a compromise, if both HDMI and SPI displays want to be used at the same time, some other compatible resolution such as 640x480 can be used. See [Raspberry Pi HDMI documentation](https://www.raspberrypi.org/documentation/configuration/config-txt/video.md) for the available options to do this.

##### Tuning Performance

The refresh speed of the display is dictated by the clock speed of the SPI bus that the display is connected to. Due to the way the BCM2835 chip on Raspberry Pi works, there does not exist a simple `speed=xxx Mhz` option that could be set to define the bus speed. Instead, the SPI bus speed is derived from two separate parameters: the core frequency of the BCM2835 SoC in general (`core_freq` in `/boot/config.txt`), and the SPI peripheral `CDIV` (Clock DIVider) setting. Together, the resulting SPI bus speed is then calculated with the formula `SPI_speed=core_freq/CDIV`.

To optimize the display to run as fast as possible,

1. Adjust the `CDIV` value by passing the directive `-DSPI_BUS_CLOCK_DIVISOR=number` in CMake command line. Possible values are even numbers `2`, `4`, `6`, `8`, `...`. Note that since `CDIV` appears in the denominator in the formula for `SPI_speed`, smaller values result in higher bus speeds, whereas higher values make the display go slower. Initially when you don't know how fast your display can run, try starting with a safe high setting, such as `-DSPI_BUS_CLOCK_DIVISOR=30`, and work your way to smaller numbers to find the maximum speed the display can cope with. See the table at the end of the README for specific observed maximum bus speeds for different displays.

2. Ensure turbo speed. This is critical for good frame rates. On the Raspberry Pi 3 Model B, the BCM2835 core runs by default at 400MHz (resulting in `400/CDIV` MHz SPI speed) **if** there is enough power provided to the Pi, and if the CPU temperature does not exceed thermal limits. If the CPU is idle, or voltage is low, the BCM2835 core will instead revert to non-turbo 250MHz state, resulting in `250/CDIV` MHz SPI speed. This effect of turbo speed on performance is significant, since 400MHz vs non-turbo 250MHz comes out to +60% of more bandwidth. Getting 60fps in Quake, Sonic or Tyrian often requires this turbo frequency, but e.g. NES and C64 emulated games can often reach 60fps even with the stock 250MHz. If for some reason under-voltage protection is kicking in even when enough power should be fed, you can [force-enable turbo when low voltage is present](https://www.raspberrypi.org/forums/viewtopic.php?f=29&t=82373) by setting the value `avoid_warnings=2` in the file `/boot/config.txt`. 

3. Perhaps a bit counterintuitively, **underclock** the core. Setting a **smaller** core frequency than the default turbo 400MHz can enable using a smaller clock divider to get a higher resulting SPI bus speed. For example, if with default `core_freq=400` SPI `CDIV=8` works (resulting in SPI bus speed `400MHz/8=50MHz`), but `CDIV=6` does not (`400MHz/6=66.67MHz` was too much), you can try lowering `core_freq=360` and set `CDIV=6` to get an effective SPI bus speed of `360MHz/6=60MHz`, a middle ground between the two that might perhaps work. Balancing `core_freq=` and `CDIV` options allows one to find the maximum SPI bus speed up to the last few kHz that the display controller can tolerate. One can also try the opposite direction and overclock, but that does then of course have all the issues that come along when overclocking. Underclocking does have the drawback that it makes the Pi run slower overall, so this is certainly a tradeoff.

##### Tuning CPU Usage

On the other hand, it is desirable to control how much CPU time fbcp-ili9341 is allowed to use. The default build settings are tuned to maximize the display refresh rate at the expense of power consumption on Pi 3B. On Pi Zero, the opposite is done, i.e. by default the driver optimizes for battery saving instead of maximal display update speed. The following options can be controlled to balance between these two:

- The main option to control CPU usage vs performance aspect is the option `#define ALL_TASKS_SHOULD_DMA` in `config.h`. Enabling this option will greatly reduce CPU usage. If this option is disabled, SPI bus utilization is maximized but CPU usage can be up to 80%-120%. When this option is enabled, CPU usage is generally up to around 15%-30%. Maximal CPU usage occurs when watching a video, or playing a fast moving game. If nothing is changing on the screen, CPU consumption of the driver should go down very close to 0-5%. By default `#define ALL_TASKS_SHOULD_DMA` is enabled for Pi Zero, but disabled for Pi 3B.

- The CMake option `-DUSE_DMA_TRANSFERS=ON` should always be enabled for good low CPU usage. If DMA transfers are disabled, the driver will run in Polled SPI mode, which generally utilizes a full dedicated single core of CPU time. If DMA transfers are causing issues, try adjusting the DMA send and receive channels to use for SPI communication with `-DDMA_TX_CHANNEL=<num>` and `-DDMA_RX_CHANNEL=<num>` CMake options.

- The statistics overlay prints out quite detailed information about execution state. Disabling the overlay with `-DSTATISTICS=0` option to CMake improves performance and reduces CPU usage. If you want to keep printing statistics, you can try increasing the interval with the `#define STATISTICS_REFRESH_INTERVAL <timeInMicroseconds>` option in config.h.

- Enabling `#define USE_GPU_VSYNC` reduces CPU consumption, but because of https://github.com/raspberrypi/userland/issues/440 can cause stuttering. Disabling `#defined USE_GPU_VSYNC` produces less stuttering, but because of https://github.com/raspberrypi/userland/issues/440, increases CPU power consumption.

- The option `#define SELF_SYNCHRONIZE_TO_GPU_VSYNC_PRODUCED_NEW_FRAMES` can be used in conjunction with `#define USE_GPU_VSYNC` to try to find a middle ground between https://github.com/raspberrypi/userland/issues/440 issues - moderate to little stuttering while not trying to consume too much CPU. Try experimenting with enabling or disabling this setting.

- There are a number of `#define SAVE_BATTERY_BY_x` options in config.h, which all default to being enabled. These should be safe to use always without tradeoffs. If you are experiencing latency or performance related issues, you can try to toggle these to troubleshoot.

- The option `#define DISPLAY_FLIP_ORIENTATION_IN_SOFTWARE` does cause a bit of extra CPU usage, so disabling it will lighten up the CPU load a bit.

- If your SPI display bus is able to run really fast in comparison to the size of the display and the amount of content changing on the screen, you can try enabling `#define UPDATE_FRAMES_IN_SINGLE_RECTANGULAR_DIFF` option in `config.h` to reduce CPU usage at the expense of increasing the number of bytes sent over the bus. This has been observed to have a big effect on Pi Zero, so is worth checking out especially there.

- If the SPI display bus is able to run really really really fast (or you don't care about frame rate, but just about low CPU usage), you can try enabling `#define UPDATE_FRAMES_WITHOUT_DIFFING` option in `config.h` to forgo the adaptive delta diffing option altogether. This will revert to naive full frame updates for absolutely minimum overall CPU usage.

- The option `#define RUN_WITH_REALTIME_THREAD_PRIORITY` can be enabled to make the driver run at realtime process priority. This can lock up the system however, but still made available for advanced experimentation.

- In `display.h` there is an option `#define TARGET_FRAME_RATE <number>`. Setting this to a smaller value, such as 30, will trade refresh rate to reduce CPU consumption.

### About Input Latency

A pleasing aspect of fbcp-ili9341 is that it introduces very little latency overhead: on a 119Hz refreshing ILI9341 display, [fbcp-ili9341 gets pixels as response from GPIO input to screen in well less than 16.66 msecs](https://www.youtube.com/watch?v=EOICdpjiqv8) time. I only have a 120fps recording camera, so can't easily measure delays shorter than that, but rough statistical estimate of slow motion video footage suggests this delay could be as low as 2-3 msecs, dominated by the ~8.4msecs panel refresh rate of the ILI9341.

This does not mean that overall input to display latency in games would be so immediate. Briefly testing a NES emulated game in Retropie suggests a total latency of about 60-80 msecs. This latency is caused by the NES game emulator overhead and extra latency added by Linux, DispmanX and GPU rendering, and [GPU framebuffer snapshotting](https://github.com/raspberrypi/userland/issues/440). (If you ran fbcp-ili9341 as a static library bypassing DispmanX and the GPU stack, directly linking your GPIO input and application logic into fbcp-ili9341, you would be able to get down to this few msecs of overall latency, like shown in the above GPIO input video)

Interestingly, fbcp-ili9341 is about [~33msecs faster than a cheap 3.5" KeDei HDMI display](https://www.youtube.com/watch?v=1yvmvv0KtNs). I do not know if this is a result of the KeDei HDMI display specifically introducing extra latency, or if all HDMI displays connected to the Pi would have similar latency overhead. An interesting question is also how SPI would compare with DPI connected displays on the Pi.

### About Tearing

Unfortunately a limitation of SPI connected displays is that the VSYNC line signal is not available on the display controllers when they are running in SPI mode, so it is not possible to do vsync locked updates even if the SPI bus bandwidth on the display was fast enough. For example, the 4 ILI9341 displays I have can all be run faster than 75MHz so SPI bus bandwidth-wise all of them would be able to update a full frame in less than a vsync interval, but it is not possible to synchronize the updates to vsync since the display controllers do not report it. (If you do know of a display that does actually expose a vsync clock signal even in SPI mode, you can try implementing support to locking on to it)

You can however choose between two distinct types of tearing artifacts: *straight line tearing* and *diagonal tearing*. Whichever looks better is a bit subjective, which is why both options exist. I prefer the straight line tearing artifact, it seems to be less intrusive than the diagonal tearing one. To toggle this, edit the option `#define DISPLAY_FLIP_ORIENTATION_IN_SOFTWARE` in `config.h`. When this option is enabled, fbcp-ili9341 produces straight line tearing, and consumes a tiny few % more CPU power. By default Pi 3B builds with straight line tearing, and Pi Zero with the faster diagonal tearing. Check out the video [Latency and tearing test #2: GPIO input to display latency in fbcp-ili9341 and tearing modes](https://www.youtube.com/watch?v=EOICdpjiqv8) to see in slow motion videos how these two tearing modes look like.

Another option that is known to affect how the tearing artifact looks like is the internal panel refresh rate. For ILI9341 displays this refresh rate can be adjusted in `ili9341.h`, and this can be set to range between `ILI9341_FRAMERATE_61_HZ` and `ILI9341_FRAMERATE_119_HZ` (default). Slower refresh rates produce less tearing, but have higher input-to-display latency, whereas higher refresh rates will result in the opposite. Again visually the resulting effect is a bit subjective.

To get tearing free updates, you should use a DPI display, or a good quality HDMI display. Beware that [cheap small 3.5" HDMI displays such as KeDei do also tear](https://www.youtube.com/watch?v=1yvmvv0KtNs) - that is, even if they are controlled via HDMI, they don't actually seem to implement VSYNC timed internal operation.

### About Smoothness

Having no vsync is not all bad though, since with the lack of vsync, SPI displays have the opportunity to obtain smoother animation on content that is not updating at 60Hz. It is possible that content on the SPI display will stutter even less than what DPI or HDMI displays on the Pi can currently provide (although I have not been able to test this in detail, except for the KeDei case above).

The main option that affects smoothness of display updates is the `#define USE_GPU_VSYNC` line in `config.h`. If this is enabled, then the internal Pi GPU HDMI vsync clock is used to drive frames onto the display. The Pi GPU clock runs at a fixed rate that is independent of the content. This rate can be discovered by running `tvservice -s` on the Pi console, and is usually 59Hz or 60Hz. If your application renders at this rate, animation will look smooth, but if not, there will be stuttering. For example playing a PAL NES game that updates at 50Hz with HDMI clock set at 60Hz will cause bad microstuttering in video output if `#define USE_GPU_VSYNC` is enabled.

If `USE_GPU_VSYNC` is disabled, then a busy spinning GPU frame snapshotting thread is used to drive the updates. This will produce smoother animation in content that does not maintain a fixed 60Hz rate. Especially in OpenTyrian, a game that renders at a fixed 36fps and has slowly scrolling scenery, the stuttering caused by `USE_GPU_VSYNC` is particularly visible. Running on Pi 3B without `USE_GPU_VSYNC` enabled produces visually smoother looking scrolling on an Adafruit 2.8" ILI9341 PiTFT set to update at 119Hz, compared to enabling `USE_GPU_VSYNC` on the same setup. Without `USE_GPU_VSYNC`, the dedicated frame polling loop thread "finds" the 36Hz update rate of the game, and then pushes pixels to the display at this exact rate. This works nicely since SPI displays disregard vsync - the result is that frames are pushed out to the SPI display immediately as they become available, instead of pulling them at a fixed 60Hz rate like HDMI does.

A drawback is that this kind of polling consumes more CPU time than the vsync option. The extra overhead is around +34% of CPU usage compared to the vsync method. It also requires using a background thread, and because of this, it is not feasible to be used on a single core Pi Zero. [If this polling was unnecessary](https://github.com/raspberrypi/userland/issues/440), this mode would also work on a Pi Zero, and without the added +34% CPU overhead on Pi 3B. See the Known Issues section below for more details.

![PiTFT display](/framerate_smoothness.jpg "Smoothness statistics")

There are two other main options that affect frame delivery timings, `#define SELF_SYNCHRONIZE_TO_GPU_VSYNC_PRODUCED_NEW_FRAMES` and `#define SAVE_BATTERY_BY_PREDICTING_FRAME_ARRIVAL_TIMES`. Check out the video [fbcp-ili9341 frame delivery smoothness test on Pi 3B and Adafruit ILI9341 at 119Hz](https://youtu.be/IqzKT33Rwjc) for a detailed side by side comparison of these different modes. The conclusions drawn from the four tested scenarios in the video are:

**1. vc_dispmanx_vsync_callback() (top left)**, set `#define USE_GPU_VSYNC` and unset `#define SELF_SYNCHRONIZE_TO_GPU_VSYNC_PRODUCED_NEW_FRAMES`:

This mode uses the DispmanX HDMI vsync signal callback to drive frames to the display.

Pros:
 - least CPU overhead if content runs at 60Hz
 - works on Pi Zero

Cons:
 - animation stutters badly on content that is < 60Hz  but also on 60Hz content
 - excessive +1 vsync interval input to display latency
 - wastes CPU overhead if content runs at less than 60Hz

**2. vc_dispmanx_vsync_callback() + self synchronization (top right)**, set `#define USE_GPU_VSYNC` and `#define SELF_SYNCHRONIZE_TO_GPU_VSYNC_PRODUCED_NEW_FRAMES`:

This mode uses the GPU vsync signal, but also aims to find and synchronize to the edge trigger when content is producing frames. This is the default build mode on Pi Zero.

Pros:
 - works on Pi Zero
 - reduced input to display latency compared to previous mode
 - content that runs at 60hz stutters less

Cons:
 - content that runs < 60 Hz still stutters badly
 - wastes CPU overhead if content runs at less than 60Hz
 - consumes slightly extra CPU compared to previous method

**3. gpu polling thread + sleep heuristic (bottom left)**, unset `#define USE_GPU_VSYNC` and set `#define SAVE_BATTERY_BY_PREDICTING_FRAME_ARRIVAL_TIMES`:

This mode runs a dedicated background thread that drives frames from the GPU to the SPI display. This is the default build mode on Pi 3B.

Pros:
 - smooth animation at all content frame rates
 - low input to display latency

Cons:
 - uses excessive CPU time, around +34% more CPU than the vsync signal based approach
 - uses excessive GPU time, the VideoCore GPU will be downscaling and snapshotting redundant frames
 - when content changes frame rate, has difficulties to adjust quickly - takes a bit of time to ramp to the new frame rate
 - requires a continuously running background thread, not feasible on Pi Zero

**4. gpu polling thread without sleeping (bottom right)**, unset `#define USE_GPU_VSYNC` and unset `#define SAVE_BATTERY_BY_PREDICTING_FRAME_ARRIVAL_TIMES`:

This mode runs the dedicated GPU thread as fast as possible, without attempting to sleep CPU.

Pros:
 - smoothest animation at all content frame rates
 - lowest input to display latency
 - adapts instantaneously to variable frame rate content

Cons:
 - uses ridiculously much CPU overhead, a full 100% core
 - uses ridiculously much GPU overhead, the VideoCore GPU will be very busy downscaling and snapshotting redundant frames
 - requires a continuously running background thread, not feasible on Pi Zero

### Known Issues

Be aware of the following limitations:

###### No rendered frame delivery via events from VideoCore IV GPU
 - The codebase captures screen framebuffers by snapshotting via the VideoCore `vc_dispmanx_snapshot()` API, and the obtained pixels are then routed on to the SPI-based display. This kind of polling is performed, since there does not exist an event-based mechanism to get new frames from the GPU as they are produced. The result is inefficient and can easily cause stuttering, since different applications produce frames at different paces. **Ideally the code would ask the VideoCore API to receive finished frames in callback notifications immediately after they are rendered**, but this kind of functionality does not exist in the current GPU driver stack. In the absence of such event delivery mechanism, the code has to resort to polling snapshots of the display framebuffer using carefully timed heuristics to balance between keeping latency and stuttering low, while not causing excessive power consumption. These heuristics keep continuously guessing the update rate of the animation on screen, and they have been tuned to ensure that CPU usage goes down to 0% when there is no detected activity on screen, but it is certainly not perfect. This GPU limitation is discussed at https://github.com/raspberrypi/userland/issues/440. If you'd like to see fbcp-ili9341 operation reduce latency, stuttering and power consumption, please throw a (kind!) comment or a thumbs up emoji in that bug thread to share that you care about this, and perhaps Raspberry Pi engineers might pick the improvement up on the development roadmap. If this issue is resolved, all of the `#define USE_GPU_VSYNC`, `#define SAVE_BATTERY_BY_PREDICTING_FRAME_ARRIVAL_TIMES` and `#define SELF_SYNCHRONIZE_TO_GPU_VSYNC_PRODUCED_NEW_FRAMES` hacks from the previous section could be deleted from the driver, hopefully leading to a best of all worlds scenario without drawbacks.

###### Screen resize freezes DispmanX
 - Currently if one resizes the video frame size at runtime, this causes DispmanX API to go sideways. See https://github.com/raspberrypi/userland/issues/461 for more information. Best workaround is to set the desired screen resolution in `/boot/config.txt` and configure all applications to never change that at runtime.

###### CPU Turbo is needed for good SPI bus bandwidth
 - The speed of the SPI bus is linked to the BCM2835 core frequency. This frequency is at 250MHz by default (on e.g. Pi Zero, 3B and 3B+), and under CPU load, the core turbos up to 400MHz. This turboing directly scales up the SPI bus speed by `400/250=+60%` as well. Therefore when choosing the SPI `CDIV` value to use, one has to pick one that works for both idle and turbo clock speeds. Conversely, the BCM core reverts to non-turbo speed when there is only light CPU load active, and this slows down the display, so if an application is graphically intensive but light on CPU, the SPI display bus does not get a chance to run at maximum speeds. A way to work around this is to force the BCM core to always stay in its turbo state with `force_turbo=1` option in `/boot/config.txt`, but this has an unfortunate effect of causing the ARM CPU to always run in turbo speed as well, consuming excessive amounts of power. At the time of writing, there does not yet exist a good solution to have both power saving and good performance. This limitation is being discussed in more detail at https://github.com/raspberrypi/firmware/issues/992.

###### Raspbian + 32-bit only(?)
 - At the moment fbcp-ili9341 is only likely to work on 32-bit OSes, on Raspbian/Ubuntu/Debian family of distributions, where Broadcom and DispmanX libraries are available. 64-bit operating systems do not currently work (see [issue #43](https://github.com/juj/fbcp-ili9341/issues/43)). It should be possible to port the driver to 64-bit and other OSes, though the amount of work has not been explored.

For more known issues and limitations, check out the [bug tracker](https://github.com/juj/fbcp-ili9341/issues), especially the entries marked *retired*, for items that are beyond current scope.

### Statistics Overlay

By default fbcp-ili9341 builds with a statistics overlay enabled. See the video [fbcp-ili9341 ported to ILI9486 WaveShare 3.5" (B) SpotPear 320x480 SPI display](https://www.youtube.com/watch?v=dqOLIHOjLq4) to find details on what each field means. Build with CMake option `-DSTATISTICS=0` to disable displaying the statistics. You can also try building with CMake option `-DSTATISTICS=2` to show a more detailed frame delivery timings histogram view, see screenshot and video above.

### FAQ and Troubleshooting

#### Why is the project named fbcp-ili9341?

The `fbcp` part in the name means *framebuffer copy*; specifically for the ILI9341 controller. fbcp-ili9341 is not actually a framebuffer copying driver, it does not create a secondary framebuffer that it would copy bytes across to from the primary framebuffer. It is also no longer a driver only for the ILI9341 controller. A more appropriate name might be *userland-raspi-spi-display-driver* or something like that, but the original name stuck.

#### Does fbcp-ili9341 work on Pi Zero?

Yes, it does, although not quite as well as on Pi 3B. If you'd like it to run better on a Pi Zero, leave a thumbs up at https://github.com/raspberrypi/userland/issues/440 - hard problems are difficult to justify prioritizing unless it is known that many people care about them.

#### The driver works well, but image is upside down. How do I rotate the display?

Enable the option `#define DISPLAY_ROTATE_180_DEGREES` in `config.h`. This should rotate the SPI display to show up the other way around, while keeping the HDMI connected display orientation unchanged. Another option is to utilize a `/boot/config.txt` option [display_rotate=2](https://www.raspberrypi.org/forums/viewtopic.php?t=120793), which rotates both the SPI output and the HDMI output.

#### How exactly do I edit the build options to e.g. remove the statistics lines or change some other option?

Edit the file `config.h` in a text editor (a command line one such as `pico`, `vim`, `nano`, or SSH map the drive to your host), and find the appropriate line in the file. Add comment lines `//` in front of that text to disable the option, or remove the `//` characters to enable it.

After having edited and saved the file, reissue `make -j` in the build directory and restart fbcp-ili9341.

Some options are passed to the build from the CMake configuration script. You can run with `make VERBOSE=1` to see which configuration items the CMake build is passing. See the above *Configuring Build Options* section to customize the CMake configure items. For example, to remove the statistics overlay, pass `-DSTATISTICS=0` directive to CMake.

#### bash: cmake: command not found!

Building requires CMake to be installed on the Pi: try `sudo apt-get install cmake`.

#### When I change a CMake option on the command line, it does not seem to apply

Try deleting CMakeCache.txt between changing CMake settings.

#### Does fbcp-ili9341 work with linux command line terminal or X windowing system?

Yes, both work fine. For linux command line terminal, the `/dev/tty1` console should be set to output to Linux framebuffer 0 (`/dev/fb0`). This is the default mode of operation and there do not exist other framebuffers in a default distribution of Raspbian, but if you have manually messed with the `con2fbmap` command in your installation, you may have inadvertently changed this configuration. Run `con2fbmap 1` to see which framebuffer the `/dev/tty1` console is outputting to, it should print `console 1 is mapped to framebuffer 0`. Type `con2fbmap 1 0` to reset console 1 back to outputting to framebuffer 0.

Likewise, the X windowing system should be configured to render to framebuffer 0. This is by default the case. The target framebuffer for X windowing service is usually configured via the `FRAMEBUFFER` environment variable before launching X. If X is not working by default, you can try overriding the framebuffer by launching X with `FRAMEBUFFER=/dev/fb0 startx` instead of just running `startx`.

#### Does fbcp-ili9341 work on Raspberry Pi 1 or Pi 2?

I don't know, I don't currently have any to test. Perhaps the code does need some model specific configuration, or perhaps it might work out of the box. I only have Pi 3B, Pi 3B+, Pi Zero W and a Pi 3 Compute Module based systems to experiment on. Pi 2 B has been reported to work by users ([#17](https://github.com/juj/fbcp-ili9341/issues/17)).

#### Does fbcp-ili9341 work on display XYZ?

If the display controller is one of the currently tested ones (see the list above), and it is wired up to run using 4-line SPI, then it should work. Pay attention to configure the `Data/Control` GPIO pin number correctly, and also specify the `Reset` GPIO pin number if the device has one.

If the display controller is not one of the tested ones, it may still work if it is similar to one of the existing ones. For example, ILI9340 and ILI9341 are practically the same controller. You can just try with a specific one to see how it goes.

If fbcp-ili9341 does not support your display controller, you will have to write support for it. fbcp-ili9341 does not have a "generic SPI TFT driver routine" that might work across multiple devices, but needs specific code for each. If you have the spec sheet available, you can ask for advice, but please do not request to add support to a display controller "blind", that is not possible.

#### Does fbcp-ili9341 work with 3-wire SPI displays?

Perhaps. This is a more recent experimental feature that may not be as stable, and there are some limitations, but 3-wire ("9-bit") SPI display support is now available. If you have a 3-wire SPI display, i.e. one that does not have a Data/Control (DC) GPIO pin to connect, configure it via CMake with directive `-DGPIO_TFT_DATA_CONTROL=-1` to tell fbcp-ili9341 that it should be driving the display with 3-wire protocol.

Current limitations of 3-wire communication are:
 - The performance option `ALL_TASKS_SHOULD_DMA` is currently not supported, there is an issue with DMA chaining that prevents this from being enabled. As result, CPU usage on 3-wire displays will be slightly higher than on 4-wire displays.
 - The performance option `OFFLOAD_PIXEL_COPY_TO_DMA_CPP` is currently not supported. As a result, 3-wire displays may not work that well on single core Pis like Pi Zero.
 - This has only been tested on my Adafruit SSD1351 128x96 RGB OLED display, which can be soldered to operate in 3-wire SPI mode, so testing has not been particularly extensive.
 - Displays that have a 16-bit wide command word, such as ILI9486, do not currently work in 3-wire ("17-bit") mode. (But ILI9486L has 8-bit command word, so that does work)

#### Does fbcp-ili9341 work with I2C, DPI, MIPI DSI or USB connected displays?

No. Those are completely different technologies altogether. It should be possible to port the driver algorithm to work on I2C however, if someone is interested.

#### Does fbcp-ili9341 work with touch displays?

At the moment one cannot utilize the XPT2046/ADS7846 touch controllers while running fbcp-ili9341, so touch is mutually incompatible with this driver. In order for fbcp-ili9341 to function, you will need to remove all `dtoverlay`s in `/boot/config.txt` related to touch.

#### Is it possible to break my display with this driver if I misconfigure something?

I have done close to everything possible to my displays - cut power in middle of operation, sent random data and command bytes, set their operating voltage commands and clock timings to arbitrary high and low values, tested unspecified and reserved command fields, and driven the displays dozens of MHz faster than they managed to keep up with, and I have not yet done permanent damage to any of my displays or Pis.

Easiest way to do permanent damage is to fail at wiring, e.g. drive 5 volts if your display requires 3.3v, or short a connection, or something similar.

The one thing that fbcp-ili9341 stays clear off is that it does not program the non-volatile memory areas of any of the displays. Therefore a hard power off on a display should clear all performed initialization and reset the display to its initial state at next power on.

That being said, if it breaks, you'll get to purchase a new shiny one to replace it.

#### Can I have both the HDMI and SPI connected at the same time?

Yes, fbcp-ili9341 shows the output of the HDMI display on the SPI screen, and both can be attached at the same time. A HDMI display does not have to be connected however, although fbcp-ili9341 operation will still be affected by whatever HDMI display mode is configured. Check out `tvservice -s` on the command line to check what the current DispmanX HDMI output mode is.

#### Do I have to show the same image on HDMI output and the SPI display, or can they be different?

At the moment fbcp-ili9341 has been developed to only display the contents of the main DispmanX GPU framebuffer over to the SPI display. That is, the SPI display will show the same picture as the HDMI output does. There is no technical restriction that requires this though, so if you know C/C++ well, it should be a manageable project to turn fbcp-ili9341 to operate as an offscreen display library to show a completely separate (non-GPU-accelerated) image than what the main HDMI display outputs. For example you could have two different outputs, e.g. a HUD overlay, a dashboard for network statistics, weather, temps, etc. showing on the SPI while having the main Raspberry Pi desktop on the HDMI.

In this kind of mode, you would probably strip the DispmanX bits out of fbcp-ili9341, and recast it as a static library that you would link to in your drawing application, and instead of snapshotting frames, you can then programmatically write to a framebuffer in memory from your C/C++ code.

#### I am running fbcp-ili9341 on a display that was listed above, but the display stays white after startup?

Unfortunately there are a number of things to go wrong that all result in a white screen. This is probably the hardest part to diagnose. Some ideas:

- double check the wiring,
- double check that the display controller is really what you expected. Trying to drive with the display with wrong initialization code usually results in the display not reacting, and the screen stays white,
- shut down and physically power off the Pi and the display in between multiple tests. Driving a display with a wrong initialization routine may put it in a bad state that needs a physical power off for it to reset,
- if there is a reset pin on the display, make sure to pass it in CMake line. Or alternatively, try driving fbcp-ili9341 without specifying the reset pin,
- make sure the display is configured to run 4-wire SPI mode, and not in parallel mode or 3-wire SPI mode. You may need to solder or desolder some connections or set a jumper to configure the specific driving mode. Support for 3-wire SPI displays does exist, but it is more limited and a bit experimental.

#### The display stays blank at boot without lighting up

This suggests that the power line or the backlight line might not be properly connected. Or if the backlight connects to a GPIO pin on the Pi (and not a voltage pin), then it may be that the pin is not in correct state for the backlight to turn on. Most of the LCD TFT displays I have immediately light up their backlight when they receive power. The Tontec one has a backlight GPIO pin that boots up high but must be pulled low to activate the backlight. OLED displays on the other hand seem to stay all black even after they do get power, while waiting for their initialization to be performed, so for OLEDs it may be normal for nothing to show up on the screen immediately after boot.

If the backlight connects to a GPIO pin, you may need to define `-DGPIO_TFT_BACKLIGHT=<pin>` in CMake command line or `config.h`, and edit `config.h` to enable `#define BACKLIGHT_CONTROL`.

#### The display clears from white to black after starting fbcp-ili9341, but picture does not show up?

fbcp-ili9341 runs a clear screen command at low speed as first thing after init, so if that goes through, it is a good sign. Try increasing `-DSPI_BUS_CLOCK_DIVISOR=` CMake option to a higher number to see if the display driving rate was too fast. Or try disabling DMA with `-DUSE_DMA_TRANSFERS=OFF` to see if this might be a DMA conflict.

#### Image does show up on display, but it freezes shortly afterwards

This suggests same as above, increase SPI bus divisor or troubleshoot disabling DMA. If DMA is detected to be the culprit, try changing up the DMA channels. Double check that `/boot/config.txt` does not have any `dtoverlay`s regarding other SPI display drivers or touch screen controllers, and that it does **NOT** have a `dtparam=spi=on` line in it - fbcp-ili9341 does not use the Linux kernel SPI driver.

Make sure other `fbcp` programs are not running, or that another copy of `fbcp-ili9341` is not running on the background.

#### Image does show up on display, but when I start up program XYZ, the image freezes

This is likely caused by the program resizing the video resolution at runtime, which breaks DispmanX. See https://github.com/raspberrypi/userland/issues/461 for more details.

#### The display works for some seconds or minutes, but then turns all white or black, or freezes

Check that the Pi is powered off of a power supply that can keep up with the voltage, and the low voltage icon is not showing up. (remove any `avoid_warnings=1/2` directive from `/boot/config.txt` if that was used to get rid of warnings overlay, to check that voltage is good) It has been observed that if there is not enough power supplied, the display can be the first to starve, while the Pi might keep on running fine. Try removing turbo settings or lowering the clock speed if you have overclocked to verify that the display crash is not power usage related.

Also try lowering SPI bus speed to a safe lower value, e.g. half of the maximum speed that the display was able to manage.

#### The driver is updating pixels on the display, but it looks all garbled

Double check the Data/Command (D/C) GPIO pin physically, and in CMake command line. Whenever fbcp-ili9341 refers to pin numbers, they are always specified in BCM pin numbers. Try setting a higher `-DSPI_BUS_CLOCK_DIVISOR=` value to CMake. Make sure no other `fbcp` programs or SPI drivers or dtoverlays are enabled.

#### Colors look wrong on the display

![BGR vs RGB and inverted colors](/wrong_colors.jpg)

If the color channels are mixed (red is blue, blue is red, green is green) like shown on the left image, pass the CMake option `-DDISPLAY_SWAP_BGR=ON` to the build.

If the color intensities look wrong (white is black, black is white, color looks like a negative image) like seen in the middle image, pass the CMake option `-DDISPLAY_INVERT_COLORS=ON` to the build.

If the colors looks off in some other fashion, it is possible that the display is just being driven at a too high SPI bus speed, in which case try making the display run slower by choosing a higher `-DSPI_BUS_CLOCK_DIVISOR=` option to CMake. Especially on ILI9486 displays it has been observed that the colors on the display can become distorted if the display is run too fast beyond its maximum capability.

#### Failed to allocate GPU memory!

fbcp-ili9341 needs a few megabytes of GPU memory to function if DMA transfers are enabled. The [gpu_mem](https://www.raspberrypi.org/documentation/configuration/config-txt/memory.md) boot config option dictates how much of the Pi's memory area is allocated to the GPU. By default this is 64MB, which has been observed to not leave enough memory for fbcp-ili9341 if HDMI is run at 1080p. If this error happens, try increasing GPU memory to e.g. 128MB by adding a line `gpu_mem=128` in `/boot/config.txt`.

#### It does not build, or crashes, or something is obviously out of date

As the number of supported displays, Raspberry Pi device models, Raspbian/Retropie/Lakka OS versions, accompanied C++ compiler versions and fbcp-ili9341 build options have grown in number, there is a combinatorial explosion of all possible build modes that one can put the codebase through, so it is not easy to keep every possible combo tested all the time. Something may have regressed or gotten outdated. Stay calm, and report a bug.

You can also try looking through the commit history to find changes related to your configuration combo, to see if there's a mention of a known good commit in time that should work for your case. If you get an odd compiler error on `cmake` or `make` lines, those will usually be very easy to fix, as they are most of the time a result of some configurational oversight.

#### Which SPI display should I buy to make sure it works best with fbcp-ili9341?

First, make sure the display is a 4-wire SPI and not a 3-wire one. A display is 4-wire SPI if it has a Data/Control (DC) GPIO line that needs connecting. Sometimes the D/C pin is labeled RS (Register Select). Support for 3-wire SPI displays does exist, but it is experimental and not nearly as well tested as 4-wire displays.

Second is the consideration about display speed. Below is a performance chart of the different displays I have tested. Note that these are sample sizes of one, I don't know how much sample variance there exists. Also I don't know if it is likely that there exists big differences between displays with same controller from different manufacturers. At least the different ILI9341 displays that I have are all quite consistent on performance, whether they are from Adafruit or WaveShare or from BuyDisplay.com.

| Vendor | Size | Resolution | Controller | Rated SPI Bus Speed | Obtained Bus Speed | Frame Rate |
| ------ | ---- | ---------- | ---------- | ------------------- | ------------------ | -----------|
| [Adafruit PiTFT](https://www.adafruit.com/product/1601) | 2.8" | 240x320 | ILI9341 | 10MHz | 294MHz/4=73.50MHz | 59.81 fps |
| [Adafruit PiTFT](https://www.adafruit.com/product/2315) | 2.2" | 240x320 | ILI9340 | 15.15MHz | 338MHz/4=84.50MHz | 68.76 fps |
| [Adafruit PiTFT](https://www.adafruit.com/product/2097) | 3.5" | 320x480 | HX8357D | 15.15MHz | 314MHz/6=52.33MHz | 21.29 fps |
| [Adafruit OLED](https://www.adafruit.com/product/1673) | 1.27" | 128x96 | SSD1351 |  20MHz | 360MHz/20=18.00MHz | 91.55 fps |
| [Waveshare RPi LCD (B) IPS](https://www.amazon.co.uk/dp/B01N48NOXI/ref=pe_3187911_185740111_TE_item) | 3.5" | 320x480 | ILI9486 | 15.15MHz | 255MHz/8=31.88MHz | 12.97 fps |
| [maithoga TFT LCD](https://www.aliexpress.com/item/3-5-inch-8P-SPI-TFT-LCD-Color-Screen-Module-ILI9486-Drive-IC-320-480-RGB/32828284227.html) | 3.5" | 320x480 | ILI9486L | 15.15MHz | 400MHz/8=50.00MHz | 13.56 fps* |
| [BuyDisplay.com SPI TFT](https://www.buydisplay.com/default/serial-spi-3-2-inch-tft-lcd-module-display-ili9341-power-than-sainsmart) copy #1 | 3.2" | 240x320 | ILI9341 | 10MHz | 310MHz/4=77.50MHz | 63.07 fps |
| [BuyDisplay.com SPI TFT](https://www.buydisplay.com/default/serial-spi-3-2-inch-tft-lcd-module-display-ili9341-power-than-sainsmart) copy #2 | 3.2" | 240x320 | ILI9341 | 10MHz | 300MHz/4=75.00MHz | 61.03 fps |
| [Arduino A000096 LCD](https://store.arduino.cc/arduino-lcd-screen) | 1.77" | 128x160 | ST7735R | 15.15MHz | 355MHz/6=59.16MHz | 180.56 fps |
| [Tontec MZ61581-PI-EXT 2016.1.28](https://www.ebay.com/p/Tontec-3-5-Inches-Touch-Screen-for-Raspberry-Pi-Display-TFT-Monitor-480x320-LCD/1649448059) | 3.5" | 320x480 | MZ61581 | 128MHz | 280MHz/2=140.00MHz | 56.97 fps |
| [Adafruit 240x240 Wide Angle TFT](https://www.adafruit.com/product/3787) | 1.54" | 240x240 | ST7789 | ? | 340MHz/4=85.00MHz | 92.23 fps |
| [WaveShare 240x240 Display HAT](https://www.waveshare.com/1.3inch-lcd-hat.htm) | 1.3" | 240x240 | ST7789VW | 62.5MHz | 338MHz/4=84.50MHz | 91.69 fps |
| [WaveShare 128x128 Display HAT](https://www.waveshare.com/1.44inch-lcd-hat.htm) | 1.44" | 128x128 | ST7735S | 15.15MHz | (untested) | (untested) |
| [KeDei v6.3](https://github.com/juj/fbcp-ili9341/issues/40) | 3.5" | 320x480 | MPI3501 | ? | 400MHz/12=33.333MHz | 4.8fps ** |

In this list, *Rated SPI Bus Speed* is the maximum clock speed that the display controller is rated to run at. The *Obtained Bus Speed* column lists the fastest SPI bus speed that was achieved in practice, and the `core_freq` BCM Core speed and SPI Clock Divider `CDIV` setting that was used to achieve that rate. Note how most display controllers can generally be driven much faster than what they are officially rated at in their spec sheets.

The *Frame Rate* column shows the worst case frame rate when full screen updates are being performed. This occurs for example when watching fullscreen video (that is not a flat colored cartoon). Because fbcp-ili9341 only sends over the pixels that have changed, displays such as HX8357D and ILI9486 can still be used to play many games at 60fps. Retro games work especially well.

All the ILI9341 displays work nice and super fast at ~70-80MHz. My WaveShare 3.5" 320x480 ILI9486 display runs really slow compared to its pixel resolution, ~32MHz only. See [fbcp-ili9341 ported to ILI9486 WaveShare 3.5" (B) SpotPear 320x480 SPI display](https://www.youtube.com/watch?v=dqOLIHOjLq4) for a video of this display in action. Adafruit's 320x480 3.5" HX8357D PiTFTs is ~64% faster in comparison.

The ILI9486L controller based maithoga display runs a bit faster than ILI9486 WaveShare, 50MHz versus 31.88MHz, i.e. +56.8% bandwidth increase. However fps-wise maithoga reaches only 13.56 vs WaveShare 12.97 fps, because the bandwidth advantage is fully lost in pixel format differences: ILI9486L requires transmitting 24 bits per each pixel (R6G6B6 mode), whereas ILI9486 supports 16 bits per pixel R5G6B5 mode. This is reflected in the above chart refresh rate for the maithoga display (marked with a star).

If manufacturing variances turn out not to be high between copies, and you'd like to have a bigger 320x480 display instead of a 240x320 one, then it is recommended to avoid ILI9486, they indeed are slow.

The KeDei v6.3 display with MPI3501 controller takes the crown of being horrible, in all aspects imaginable. It is able to run at 33.33 MHz, but due to technical design limitations of the display (see [#40](https://github.com/juj/fbcp-ili9341/issues/40#issuecomment-441480557)), effective bus speed is halved, and only about 72% utilization of the remaining bus rate is achieved. DMA cannot be used, so CPU usage will be off the charts. Even though fbcp-ili9341 supports this display, level of support is expected to be poor, because the hardware design is a closed secret without open documentation publicly available from the manufacturer. Stay clear of KeDei or MPI3501 displays.

The Tontec MZ61581 controller based 320x480 3.5" display on the other hand can be driven insanely fast at up to 140MHz! These seem to be quite hard to come by though and they are expensive. Tontec seems to have gone out of business and for example the domain itontec.com from which the supplied instructions sheet asks to download original drivers from is no longer registered. I was able to find one from eBay for testing.

Search around, or ask the manufacturer of the display what the maximum SPI bus speed is for the device. This is the most important aspect to getting good frame rates, but unfortunately most web links never state the SPI speed rating, or they state it ridiculously low like in the spec sheets. Try and buy to see, or ask in some community forums from people who already have a particular display to find out what SPI bus speed it can achieve.

One might think that since Pi Zero is slower than a Pi 3, the SPI bus speed might not matter as much when running on a Pi Zero, but the effect is rather the opposite. To get good framerates on a Pi Zero, it should be paired with a display with as high SPI bus speed capability as possible. This is because the higher the SPI bus speed is, the more autonomously a DMA controller can drive it without CPU intervention. For the same reason, the interlacing technique does not (currently at least) perform well on a Pi Zero, so it is disabled there by default. ILI9341s run well on Pi Zero, ILI9486 on the other hand is quite difficult to combine with a Pi Zero.

Ultimately, it should be noted that parallel displays (DPI) are the proper method for getting fast framerates easily. SPI displays should only be preferred if display form factor is important and a desired product might only exist as SPI and not as DPI, or the number of GPIO pins that are available on the Pi is scarce that sacrificing dozens of pins to RGB data is not feasible.

#### What other options/alternatives do I have to fbcp-ili9341?

Hardware-wise, there are six different ways to connect displays to the Pi. Here are the pros and cons of each:

1. [Composite video]([https://en.wikipedia.org/wiki/Composite_video](https://en.wikipedia.org/wiki/Composite_video))
     - +simple one-wire connectivity
     - +the Pi GPU drives the signal on its own without CPU assistance, no driver needed
     - +has vsync, no tearing artifacts
     - +available for a cheap price
     - -low quality analog signal that is blurry and has color artifacts
2. [I²C (Inter-Integrated Circuit)]([https://en.wikipedia.org/wiki/I%C2%B2C](https://en.wikipedia.org/wiki/I%C2%B2C))
    - +fewest amount of digital signals (two): SDA (data) and SCL (clock)
    - +available for a cheap price
    - -slowest bandwidth, generally only the smallest displays with low resolution utilize this
    - -need software CPU cycles to push pixels to display
    - -no video vsync, causes tearing artifacts
3. [SPI (Serial Peripheral Interface)]([https://en.wikipedia.org/wiki/Serial_Peripheral_Interface](https://en.wikipedia.org/wiki/Serial_Peripheral_Interface))
     - the method used/supported by this driver
     - +only few digital signal lines needed: SCLK (clock), MOSI (data), D/C (data/command) (MISO line is not read by fbcp-ili9341), CS (Chip Select) (sometimes optional)
     - +much faster than I²C
     - +very low video display latency
     - +available for a cheap price
     - -no single pin or protocol standard, be careful about hardware compatibility
     - -need software CPU cycles to push pixels to display
     - -no video vsync, causes tearing artifacts
     - -low resolution, generally 480x320 or smaller
4. [DPI (Display Parallel Interface)]([https://www.raspberrypi.org/documentation/hardware/raspberrypi/dpi/README.md](https://www.raspberrypi.org/documentation/hardware/raspberrypi/dpi/README.md))
     - +high quality digital signal driven directly by the Pi GPU without CPU assistance
     - +fixed 60hz updates without missed frames
     - +has vsync, no tearing artifacts
     - -no single pin or protocol standard, be careful about hardware compatibility
     - -consumes most of the pins on the Pi GPIO header (20-28 digital signal pins)
     - -no ability to disable vsync, likely more video latency than SPI
5. [MIPI-DSI (Display Serial Interface)]([https://en.wikipedia.org/wiki/Display_Serial_Interface](https://en.wikipedia.org/wiki/Display_Serial_Interface))
     - +high quality digital signal driven directly by the Pi GPU without CPU assistance
     - +fixed 60hz updates without missed frames
     - +has vsync, no tearing artifacts
     - +does not require GPIO pins, leaving them free for other use
     - +available in high resolution
     - [-uses proprietary DSI connectivity on the Pi, not an open ecosystem]([https://www.raspberrypi.org/forums/viewtopic.php?t=153954](https://www.raspberrypi.org/forums/viewtopic.php?t=153954))
     - [-only one official display exists]([https://www.raspberrypi.org/documentation/hardware/display/](https://www.raspberrypi.org/documentation/hardware/display/))
6. [HDMI]([https://en.wikipedia.org/wiki/HDMI](https://en.wikipedia.org/wiki/HDMI))
     - +high quality digital signal driven directly by the Pi GPU without CPU assistance
     - +fixed 60hz updates without missed frames
     - +has vsync, no tearing artifacts
     - +does not require GPIO pins, leaving them free for other use
     - +very standard, little configuration needed in /boot/config.txt
     - +available in high resolution
     - -bulky connector for most portable designs

Displays are generally manufactured to utilize one specific interfacing method, with the exception that some displays have a both I²C and SPI modes that can be configured via soldering.

Fbcp-ili9341 driver is about interfacing with SPI displays. If your display utilizes some other connection mechanism, fbcp-ili9341 will not apply.

Software-wise, there are two possible alternatives to fbcp-ili9341:

1. [notro/fbtft](https://github.com/notro/fbtft) + [tasanakorn/rpi-fbcp](https://github.com/tasanakorn/rpi-fbcp)
2. Use an ad hoc drawing library that provides both drawing primitives plus the display interface, e.g. [adafruit/Adafruit_Python_ILI9341](https://github.com/adafruit/Adafruit_Python_ILI9341).

### Resources

The following links proved helpful when writing this:
 - [ARM BCM2835 Peripherals Manual PDF](https://www.raspberrypi.org/app/uploads/2012/02/BCM2835-ARM-Peripherals.pdf),
 - [ILI9341 Display Controller Manual PDF](https://cdn-shop.adafruit.com/datasheets/ILI9341.pdf),
 - [notro/fbtft](https://github.com/notro/fbtft): Linux Framebuffer drivers for small TFT LCD display modules,
 - [BCM2835 driver](http://www.airspayce.com/mikem/bcm2835/) for Raspberry Pi,
 - [tasanakorn/rpi-fbcp](https://github.com/tasanakorn/rpi-fbcp), original framebuffer driver,
 - [tasanakorn/rpi-fbcp/#16](https://github.com/tasanakorn/rpi-fbcp/issues/16), discussion about performance,
 - [Tomáš Suk, Cyril Höschl IV, and Jan Flusser, Rectangular Decomposition of Binary Images.](http://library.utia.cas.cz/separaty/2012/ZOI/suk-rectangular%20decomposition%20of%20binary%20images.pdf), a useful research paper about merging monochrome bitmap images to rectangles, which gave good ideas for optimizing SPI span merges across multiple scan lines,
 - [VC DispmanX source code](https://github.com/raspberrypi/userland/blob/master/interface/vmcs_host/vc_vchi_dispmanx.c) (more or less the only official documentation bit on DispmanX I could ever find)

### I Want To Contribute / Future Work / TODOs

If you would like to help push Raspberry Pi SPI display support further, there are always more things to do in the project. Here is a list of ideas and TODOs for recognized work items to contribute, roughly rated in order of increasing difficulty.

 - Vote up issue [raspberrypi/userland/#440](https://github.com/raspberrypi/userland/issues/440) if you would like to see Raspberry Pi Foundation improve CPU performance and reduce latency of the Pi when used with SPI displays.
 - Vote up issue [raspberrypi/userland/#461](https://github.com/raspberrypi/userland/issues/461) if you would like to see fbcp-ili9341 not die (due to DispmanX dying) when HDMI display resolution changes.
 - Vote up issue [raspberrypi/firmware/#992](https://github.com/raspberrypi/firmware/issues/992) if you would like to see Raspberry Pi SPI bus to have high throughput even when the Pi CPU is not under heavy CPU load (better SPI throughput with lower power consumption), a performance feature only SDHOST on the Pi currently enjoys.
 - Benchmark fbcp-ili9341 performance in your use case with CPU tool `top`/`htop`, or with a power meter off the wall and report the results.
 - Do you have a display with an unlisted or unknown display controller? Post close up photos of it to an issue in the tracker, and report if you were able to make it work with fbcp-ili9341?
 - Did you have to do something unexpected or undocumented to get fbcp-ili9341 to work in your environment or use case? Write up a tutorial or record a video to let people know about the gotchas.
 - If you have access to a high frequency scope/logic analyzer (~128MHz), audit the utilization of the SPI MOSI bus to find any remaining idle times on the bus, and analyze their sources.
 - Port fbcp-ili9341 to work as a static code library that one can link to another application for CPU-based drawing directly to display, bypassing inefficiencies and latency of the general purpose Linux DispmanX/graphics stack.
 - Improve existing display initialization routines with options to control e.g. gamma curves, color saturation, driving voltages, refresh rates or other potentially useful features that the display controller protocols expose.
 - Add support to fbcp-ili9341 to a new display controller. (e.g. [#26](https://github.com/juj/fbcp-ili9341/issues/26))
 - Implement support for reading the MISO line for display identification numbers/strings for potentially interesting statistics (could some of the displays be autodetected this way?)
 - Add support for other color modes, like RGB666 or RGB888. Currently fbcp-ili9341 only knows about RGB565 display mode.
 - Implement a kernel module that enables userland programs to allocate DMA channels, which fbcp-ili9341 could use to amicably reserve its own DMA channels without danger of conflicting.
 - Implement support for touch control while fbcp-ili9341 is active. ([#33](https://github.com/juj/fbcp-ili9341/issues/33))
 - Implement support for SPI-based SD card readers that are sometimes attached to displays.
 - Port fbcp-ili9341 to work with I2C displays.
 - Port more key algorithms to ARM assembly to optimize performance of fbcp-ili9341 in hotspots, or optimize execution in some other ways?
 - Add support to building fbcp-ili9341 on another operating system than Raspbian. (see [#43](https://github.com/juj/fbcp-ili9341/issues/43))
 - Add support for building on 64-bit operating systems. (see [#43](https://github.com/juj/fbcp-ili9341/issues/43))
 - Port fbcp-ili9341 over to a new single-board computer hardware. (e.g. [#30](https://github.com/juj/fbcp-ili9341/issues/30))
 - Improve support for 3-wire displays, e.g. for 1) "17-bit" 3-wire communication, 2) fix up `SPI_3WIRE_PROTOCOL` + `ALL_TASKS_SHOULD_DMA` to work together, or 3) fix up `SPI_3WIRE_PROTOCOL` + `OFFLOAD_PIXEL_COPY_TO_DMA_CPP` to work together.
 - Optimize away unnecessary zero padding that 3-wire communication currently incurs, by keeping a queue of leftover untransmitted partial bits of a byte, and piggybacking them onto the next transfer that comes in.
 - Port the high performance DMA-based SPI communication technique from fbcp-ili9341 over to another project that uses the SPI bus for something else, for close to 100% saturation of the SPI bus in the project.
 - Improve the implementation of chaining DMA transfers to not only chain transfers within a single SPI task, but also across multiple SPI tasks.
 - Optimize `ALL_TASKS_SHOULD_DMA` mode to be always superior in performance and CPU usage so that the non-`ALL_TASKS_SHOULD_DMA` path can be dropped from the codebase. (probably requires the above chaining to function efficiently)
 - If you are knowledgeable with BCM2835 DMA, investigate whether the hacky dance where two DMA channels need to be used to reset and resume DMA SPI transfers when chaining, can be avoided?
 - If you have contacts with Broadcom, ask them to promote use of the SoC hardware with DMA chaining + mixed SPI & non-SPI tasks as a first class tested use case. Current DMA SPI hardware behavior of BCM2835 is, to say the least, surprising.

### License

This driver is licensed under the MIT License. See LICENSE.txt. In nonlegal terms, it's yours for both free and commercial projects, DIY packages, kickstarters, Etsys and Ebays, and you don't owe back a dime. Feel free to apply and derive as you wish.

If you found fbcp-ili9341 useful, it makes me happy to hear back about the projects it found a home in. If you did a build or a project where fbcp-ili9341 worked out, it'd be great to see a video or some photos or read about your experiences.

I hope you build something you enjoy!

### Donating

I have been occassionally asked how to make a donation as a thank you for the work, so here is a PayPal link:

[![paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=DD8A74WY6Q4L2&currency_code=EUR)

Please note that a contribution is not expected, and you are free to use, publicize and redistribute the driver even without a payment.

### Contacting

Best way to discuss the driver is to open a GitHub issue. You may also be able to find me over at [sudomod.com Discord channel](https://sudomod.com/forum/viewtopic.php?f=42&t=438&sid=b868bb95ab5c3035b7810c71278637c6).
