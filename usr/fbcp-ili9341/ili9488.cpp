#include "config.h"

#if defined(ILI9488)

#include "spi.h"

#include <memory.h>
#include <stdio.h>

void InitILI9488()
{
  // If a Reset pin is defined, toggle it briefly high->low->high to enable the device. Some devices do not have a reset pin, in which case compile with GPIO_TFT_RESET_PIN left undefined.
#if defined(GPIO_TFT_RESET_PIN) && GPIO_TFT_RESET_PIN >= 0
  printf("Resetting ili9488 display at reset GPIO pin %d\n", GPIO_TFT_RESET_PIN);
  SET_GPIO_MODE(GPIO_TFT_RESET_PIN, 1);
  SET_GPIO(GPIO_TFT_RESET_PIN);
  usleep(120 * 1000);
  CLEAR_GPIO(GPIO_TFT_RESET_PIN);
  usleep(120 * 1000);
  SET_GPIO(GPIO_TFT_RESET_PIN);
  usleep(120 * 1000);
#endif

  // Do the initialization with a very low SPI bus speed, so that it will succeed even if the bus speed chosen by the user is too high.
  spi->clk = 34;
  __sync_synchronize();

  BEGIN_SPI_COMMUNICATION();
  {
      //0xE0 - PGAMCTRL Positive Gamma Control
      SPI_TRANSFER(0xE0, 0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F);
      //0xE1 - NGAMCTRL Negative Gamma Control
      SPI_TRANSFER(0xE1, 0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F);
      // 0xC0 Power Control 1
      SPI_TRANSFER(0xC0, 0x17, 0x15);
      // 0xC1 Power Control 2
      SPI_TRANSFER(0xC1, 0x41);
      // 0xC5 VCOM Control
      SPI_TRANSFER(0xC5, 0x00, 0x12, 0x80);

// Memory access control. Determines display orientation,
// display color filter and refresh order/direction.
#define MADCTL_HORIZONTAL_REFRESH_ORDER (1<<2)
#define MADCTL_BGR_PIXEL_ORDER (1<<3)
#define MADCTL_VERTICAL_REFRESH_ORDER (1<<4)
#define MADCTL_ROW_COLUMN_EXCHANGE (1<<5)
#define MADCTL_COLUMN_ADDRESS_ORDER_SWAP (1<<6)
#define MADCTL_ROW_ADDRESS_ORDER_SWAP (1<<7)
#define MADCTL_ROTATE_180_DEGREES (MADCTL_COLUMN_ADDRESS_ORDER_SWAP | MADCTL_ROW_ADDRESS_ORDER_SWAP)

    uint8_t madctl(0);
#ifndef DISPLAY_SWAP_BGR
    madctl |= MADCTL_BGR_PIXEL_ORDER;
#endif
#if defined(DISPLAY_FLIP_ORIENTATION_IN_HARDWARE)
    madctl |= MADCTL_ROW_COLUMN_EXCHANGE;
#endif
#ifdef DISPLAY_ROTATE_180_DEGREES
    madctl ^= MADCTL_ROTATE_180_DEGREES;
#endif
    //
    // Shifted value of bits [7:5] (MY - ROW_ADDRESS_ORDER_SWAP, MX - COLUMN_ADDRESS_ORDER_SWAP, MV ROW_COLUMN_EXCHANGE)
    // and their resulting effect on the orientation of the image
    // relative to the physical screen:
    // 0x40 0 deg (W = 320, H = 480, FPC connector at bottom)
    // 0x20 90 deg (W = 480, H = 320, FPC connector on right)
    // 0x80 180 deg (W = 320, H = 480, FPC connector on top)
    // 0xE0 270 deg (W = 480, H = 320, FPC connector on left)
      // 0x36 Memory Access Control - sets display rotation.
      SPI_TRANSFER(0x36, madctl);

      // 0x3A Interface Pixel Format (bit depth color space)
      SPI_TRANSFER(0x3A, 0x66);
      // 0xB0 Interface Mode Control
      SPI_TRANSFER(0xB0, 0x80);
      // 0xB1 Frame Rate Control (in Normal Mode/Full Colors)
      SPI_TRANSFER(0xB1, 0xA0);

// The display inversion is controlled by two registers:
// 0xB4 determines how the LEDs are swapped.See page 224 of the datasheet:
// The different values are:
// 0x00 Column inversion.
// 0x01 1 dot inversion.
// 0x02 2 dot inversion.
// 0x20/0x21 engage and disengage the inversion itself.
// 
// I could not find a difference is using the three different
// settings for 0xB4. It is left at 0x02 since that is what
// the original test value was set to.
#ifdef DISPLAY_INVERT_COLORS
      // 0xB4 Display Inversion Control.
      SPI_TRANSFER(0xB4, 0x02);
      // 0x21 Display Inversion ON.
      SPI_TRANSFER(0x21);
#else
      // 0xB4 Display Inversion Control.
      SPI_TRANSFER(0xB4, 0x02);
      // 0x20 Display Inversion OFF.
      SPI_TRANSFER(0x20);
#endif

      // 0xB6 Display Function Control.
      SPI_TRANSFER(0xB6, 0x02, 0x02);
      // 0xE9 Set Image Function.
      SPI_TRANSFER(0xE9, 0x00);
      // 0xF7 Adjuist Control 3
      SPI_TRANSFER(0xF7, 0xA9, 0x51, 0x2C, 0x82);
      // 0x11 Exit Sleep Mode. (Sleep OUT)
      SPI_TRANSFER(0x11);
      usleep(120*1000);
      // 0x29 Display ON.
      SPI_TRANSFER(0x29);
      // 0x38 Idle Mode OFF.
      SPI_TRANSFER(0x38);
      // 0x13 Normal Display Mode ON.
      SPI_TRANSFER(0x13);

#if defined(GPIO_TFT_BACKLIGHT) && defined(BACKLIGHT_CONTROL)
    printf("Setting TFT backlight on at pin %d\n", GPIO_TFT_BACKLIGHT);
    TurnBacklightOn();
#endif

    ClearScreen();
  }
#ifndef USE_DMA_TRANSFERS // For DMA transfers, keep SPI CS & TA active.
  END_SPI_COMMUNICATION();
#endif

  // And speed up to the desired operation speed finally after init is done.
  usleep(10 * 1000); // Delay a bit before restoring CLK, or otherwise this has been observed to cause the display not init if done back to back after the clear operation above.
  spi->clk = SPI_BUS_CLOCK_DIVISOR;
}

void TurnBacklightOff()
{
#if defined(GPIO_TFT_BACKLIGHT) && defined(BACKLIGHT_CONTROL)
  SET_GPIO_MODE(GPIO_TFT_BACKLIGHT, 0x01); // Set backlight pin to digital 0/1 output mode (0x01) in case it had been PWM controlled
  CLEAR_GPIO(GPIO_TFT_BACKLIGHT); // And turn the backlight off.
#endif
}

void TurnBacklightOn()
{
#if defined(GPIO_TFT_BACKLIGHT) && defined(BACKLIGHT_CONTROL)
  SET_GPIO_MODE(GPIO_TFT_BACKLIGHT, 0x01); // Set backlight pin to digital 0/1 output mode (0x01) in case it had been PWM controlled
  SET_GPIO(GPIO_TFT_BACKLIGHT); // And turn the backlight on.
#endif
}

void TurnDisplayOff()
{
  TurnBacklightOff();
  QUEUE_SPI_TRANSFER(0x28/*Display OFF*/);
  QUEUE_SPI_TRANSFER(0x10/*Enter Sleep Mode*/);
  usleep(120*1000); // Sleep off can be sent 120msecs after entering sleep mode the earliest, so synchronously sleep here for that duration to be safe.
}

void TurnDisplayOn()
{
  TurnBacklightOff();
  QUEUE_SPI_TRANSFER(0x11/*Sleep Out*/);
  usleep(120 * 1000);
  QUEUE_SPI_TRANSFER(0x29/*Display ON*/);
  usleep(120 * 1000);
  TurnBacklightOn();
}

void DeinitSPIDisplay()
{
  ClearScreen();
  TurnDisplayOff();
}

#endif
