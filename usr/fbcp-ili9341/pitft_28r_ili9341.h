#pragma once

// Data specific to Adafruit's PiTFT 2.8 display
#ifdef ADAFRUIT_ILI9341_PITFT

// Even though the display controller protocol over the SPI bus is standard as per e.g. ILI9341 spec sheet,
// and the pins that one uses for the Pi hardware SPI are also standard;
// the choice of which Raspberry Pi GPIO pin is used for flipping the Data/Control pin of the display
// can vary. Pre-made stack-on hats such as on Adafruit's ILI9341, or predesigned schematics configurations
// such as Freeplaytech's WaveShare32B display wiring can standardize the pin to use in some configurations, but
// if you did your wiring customized directly on the GPIO pins, you will likely need to check which pin to
// configure here. This pin numberings is specified in the BCM pins namespace.

#if !defined(GPIO_TFT_DATA_CONTROL)
#define GPIO_TFT_DATA_CONTROL 25  /*!< Version 1, Pin P1-22, PiTFT 2.8 resistive Data/Control pin */
#endif

#if !defined(GPIO_TFT_BACKLIGHT)
// Adafruit 2.2" 320x240 HAT has backlight on pin 18: https://learn.adafruit.com/adafruit-2-2-pitft-hat-320-240-primary-display-for-raspberry-pi/backlight-control
// So does Adafruit 2.8" 320x240 display: https://learn.adafruit.com/adafruit-pitft-28-inch-resistive-touchscreen-display-raspberry-pi/backlight-control
// And so does Adafruit 3.5" 480x320 display: https://learn.adafruit.com/adafruit-pitft-3-dot-5-touch-screen-for-raspberry-pi/faq?view=all#pwm-backlight-control-with-gpio-18
#define GPIO_TFT_BACKLIGHT 18
#endif

#endif
