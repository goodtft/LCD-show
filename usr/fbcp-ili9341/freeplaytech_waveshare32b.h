#pragma once

// Data specific to the Waveshare32b display, as present on FreePlayTech's CM3/Zero devices (https://www.freeplaytech.com/)
#ifdef FREEPLAYTECH_WAVESHARE32B

#if !defined(GPIO_TFT_DATA_CONTROL)
#define GPIO_TFT_DATA_CONTROL 22
#endif

#if !defined(GPIO_TFT_RESET_PIN)
#define GPIO_TFT_RESET_PIN 27
#endif

// On FreePlayTech GBA devices, a part of the screen is hidden by the bezels, since the GBA has a 2.8" screen surface area, but a 3.2" display is enclosed inside the case.
// The hidden area is placed under the left edge of the display horizontally, and under top and bottom edges of the display vertically, so reduce those out from the drawable area.
// Effective display area is then 320-18=302 pixels horizontally, and 202 pixels vertically (in landscape direction).

// The meaning of top/left/right/bottom here should be interpreted as the display being oriented in its native direction (which is portrait mode for ILI9341, 240x320 direction).
#define DISPLAY_NATIVE_COVERED_TOP_SIDE 18
#define DISPLAY_NATIVE_COVERED_LEFT_SIDE 9
#define DISPLAY_NATIVE_COVERED_RIGHT_SIDE 29
#define DISPLAY_NATIVE_COVERED_BOTTOM_SIDE 0

#endif
