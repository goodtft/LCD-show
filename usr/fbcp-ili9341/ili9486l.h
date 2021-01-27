#pragma once

#include "config.h"

// On my Maithoga ILI9486L display (https://www.aliexpress.com/item/3-5-inch-8P-SPI-TFT-LCD-Color-Screen-Module-ILI9486-Drive-IC-320-480-RGB/32828284227.html), the following speed settings have been tested:

// core_freq=400: CDIV=8, results in 50.00MHz, works
// core_freq=400: CDIV=6, would result in 66.666MHz, but this is too fast for the display, and produces corrupted output
// core_freq=305: CDIV=6, would result in 50.833MHz, but this would work most of the time, producing occassional corrupted pixels

// Data specific to the ILI9486L controller
#define DISPLAY_SET_CURSOR_X 0x2A
#define DISPLAY_SET_CURSOR_Y 0x2B
#define DISPLAY_WRITE_PIXELS 0x2C

#define DISPLAY_NATIVE_WIDTH 320
#define DISPLAY_NATIVE_HEIGHT 480

// ILI9486L only supports 18 bits/pixel R6G6B6 format (padded to 3 bytes per pixel), and no 16-bits R5G6B5 mode.
#define DISPLAY_COLOR_FORMAT_R6X2G6X2B6X2

// ILI9486L does not behave well if one sends partial commands, but must finish each command or the command does not apply
#define MUST_SEND_FULL_CURSOR_WINDOW

void InitILI9486(void);
#define InitSPIDisplay InitILI9486
