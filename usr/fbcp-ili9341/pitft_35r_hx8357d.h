#pragma once

// Data specific to Adafruit's PiTFT 3.5" display
#ifdef ADAFRUIT_HX8357D_PITFT

// SPI_BUS_CLOCK_DIVISOR specifies how fast to communicate the SPI bus at. Possible values
// are 4, 6, 8, 10, 12, ... Smaller values are faster. On my PiTFT 3.5" display, the
// following values were observed to work (on a Pi 3B):

// core_freq=314: CDIV=6, results in 52.333MHz

// While the following values were seen to not work:

// core_freq=315: CDIV=6, would result in 52.50MHz, which would work for several minutes, but then introduce infrequent single pixel glitches

#if !defined(GPIO_TFT_DATA_CONTROL)
#define GPIO_TFT_DATA_CONTROL 25
#endif

#if !defined(GPIO_TFT_BACKLIGHT)
// Adafruit 2.2" 320x240 HAT has backlight on pin 18: https://learn.adafruit.com/adafruit-2-2-pitft-hat-320-240-primary-display-for-raspberry-pi/backlight-control
// So does Adafruit 2.8" 320x240 display: https://learn.adafruit.com/adafruit-pitft-28-inch-resistive-touchscreen-display-raspberry-pi/backlight-control
// And so does Adafruit 3.5" 480x320 display: https://learn.adafruit.com/adafruit-pitft-3-dot-5-touch-screen-for-raspberry-pi/faq?view=all#pwm-backlight-control-with-gpio-18
#define GPIO_TFT_BACKLIGHT 18
#endif

#endif
