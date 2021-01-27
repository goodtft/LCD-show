#pragma once

// Data specific to the Waveshare35b display
#ifdef WAVESHARE35B_ILI9486

// SPI_BUS_CLOCK_DIVISOR specifies how fast to communicate the SPI bus at. Possible values
// are 4, 6, 8, 10, 12, ... Smaller values are faster. On my Waveshare35b display, the
// following values were observed to work (on a Pi 3B):

// core_freq=400: CDIV=14, results in 28.57MHz
// core_freq=255: CDIV=8, results in 31.875MHz

// While the following values were seen to not work:

// core_freq=400: CDIV=12, would result in 33.33MHz, but this was too fast for the display
// core_freq=256: CDIV=8, would result in 32.00MHz, this would work 99% of the time, but occassionally every ~few minutes would glitch a pixel or two

#if !defined(GPIO_TFT_DATA_CONTROL)
#define GPIO_TFT_DATA_CONTROL 24
#endif

#if !defined(GPIO_TFT_RESET_PIN)
#define GPIO_TFT_RESET_PIN 25
#endif

#endif
