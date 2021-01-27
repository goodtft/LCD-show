#pragma once

#if defined(ILI9341) || defined(ILI9340)

// SPI_BUS_CLOCK_DIVISOR specifies how fast to communicate the SPI bus at. Possible values are 4, 6, 8, 10, 12, ... Smaller
// values are faster. On my PiTFT 2.8 and Waveshare32b displays, divisor value of 4 does not work, and
// 6 is the fastest possible. While developing, it was observed that a value of 12 or higher did not
// actually work either, and only 6, 8 and 10 were functioning properly.

// On Adafruit PiTFT 2.8", the following speed configurations have been tested (on a Pi 3B):
// core_freq=400: CDIV=6, results in 66.67MHz, works
// core_freq=294: CDIV=4, results in 73.50MHz, works
// core_freq=320: CDIV=4, would result in 80.00MHz, but this was too fast for the display
// core_freq=300: CDIV=4, would result in 75.00MHz, and would work for ~99% of the time, but develop rare occassional pixel glitches once a minute or so.
// core_freq=296: CDIV=4, would result in 74.50MHz, would produce tiny individual pixel glitches very rarely, once every few 10 minutes or so.

// On Waveshare 3.2", the following speed configurations have been observed to work (on a Pi 3B):
// core_freq=400: CDIV=6, results in 66.67MHz, works
// core_freq=310: CDIV=4, results in 77.50MHz, works
// core_freq=320: CDIV=4, would result in 80.00MHz, would work most of the time, but produced random occassional glitches every few minutes or so.

// On Adafruit 2.2" PiTFT HAT - 320x240 Display with ILI9340 controller, the following speed configurations have been tested (on a Pi 3B):
// core_freq=338: CDIV=4, results in 84.5MHz, works
// core_freq=339: CDIV=4, would result in 84.75MHz, would work most of the time, but every few minutes generated random glitched pixels.

// Data specific to the ILI9341 controller
#define DISPLAY_SET_CURSOR_X 0x2A
#define DISPLAY_SET_CURSOR_Y 0x2B
#define DISPLAY_WRITE_PIXELS 0x2C

// ILI9341 displays are able to update at any rate between 61Hz to up to 119Hz. Default at power on is 70Hz.
#define ILI9341_FRAMERATE_61_HZ 0x1F
#define ILI9341_FRAMERATE_63_HZ 0x1E
#define ILI9341_FRAMERATE_65_HZ 0x1D
#define ILI9341_FRAMERATE_68_HZ 0x1C
#define ILI9341_FRAMERATE_70_HZ 0x1B
#define ILI9341_FRAMERATE_73_HZ 0x1A
#define ILI9341_FRAMERATE_76_HZ 0x19
#define ILI9341_FRAMERATE_79_HZ 0x18
#define ILI9341_FRAMERATE_83_HZ 0x17
#define ILI9341_FRAMERATE_86_HZ 0x16
#define ILI9341_FRAMERATE_90_HZ 0x15
#define ILI9341_FRAMERATE_95_HZ 0x14
#define ILI9341_FRAMERATE_100_HZ 0x13
#define ILI9341_FRAMERATE_106_HZ 0x12
#define ILI9341_FRAMERATE_112_HZ 0x11
#define ILI9341_FRAMERATE_119_HZ 0x10

// Visually estimating NES Super Mario Bros 3 "match mushroom, flower, star" arcade game, 119Hz gives visually
// most pleasing result, so default to using that. You can also try other settings above. 119 Hz should give
// lowest latency, perhaps 61 Hz might give least amount of tearing, although this can be quite subjective.
#define ILI9341_UPDATE_FRAMERATE ILI9341_FRAMERATE_119_HZ

// Appears in ILI9341 Data Sheet v1.11 (2011/06/10), but not in older v1.02 (2010/12/06). This has a subtle effect on colors/saturation.
// Valid values are 0x20 and 0x30. Spec says 0x20 is default at boot, but doesn't seem so, more like 0x00 is default, giving supersaturated colors. I like 0x30 best.
// Value 0x30 doesn't seem to be available on ILI9340.
#define ILI9341_PUMP_CONTROL_2XVCI 0x20
#define ILI9341_PUMP_CONTROL_3XVCI 0x30

#ifdef ILI9341
#define ILI9341_PUMP_CONTROL ILI9341_PUMP_CONTROL_3XVCI 
#else
#define ILI9341_PUMP_CONTROL ILI9341_PUMP_CONTROL_2XVCI
#endif


#define DISPLAY_NATIVE_WIDTH 240
#define DISPLAY_NATIVE_HEIGHT 320

#ifdef ADAFRUIT_ILI9341_PITFT
#include "pitft_28r_ili9341.h"
#elif defined(ADAFRUIT_HX8357D_PITFT)
#include "pitft_35r_hx8357d.h"
#elif defined(FREEPLAYTECH_WAVESHARE32B)
#include "freeplaytech_waveshare32b.h"
#endif

#define InitSPIDisplay InitILI9341

void InitILI9341(void);

#endif
