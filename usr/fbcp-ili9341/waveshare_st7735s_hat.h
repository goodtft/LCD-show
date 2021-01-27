#pragma once

// Data specific to WaveShare 128x128, 1.44inch LCD ST7735S hat, https://www.waveshare.com/1.44inch-lcd-hat.htm
#ifdef WAVESHARE_ST7735S_HAT

#if !defined(GPIO_TFT_DATA_CONTROL)
#define GPIO_TFT_DATA_CONTROL 25
#endif

#if !defined(GPIO_TFT_BACKLIGHT)
#define GPIO_TFT_BACKLIGHT 24
#endif

#if !defined(GPIO_TFT_RESET_PIN)
#define GPIO_TFT_RESET_PIN 27
#endif

#define DISPLAY_SHOULD_FLIP_ORIENTATION

#endif
