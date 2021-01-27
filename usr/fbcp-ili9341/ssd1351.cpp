#include "config.h"

#ifdef SSD1351

#include "spi.h"

#include <memory.h>
#include <stdio.h>

void InitSSD1351()
{
  // If a Reset pin is defined, toggle it briefly high->low->high to enable the device. Some devices do not have a reset pin, in which case compile with GPIO_TFT_RESET_PIN left undefined.
#if defined(GPIO_TFT_RESET_PIN) && GPIO_TFT_RESET_PIN >= 0
  printf("Resetting display at reset GPIO pin %d\n", GPIO_TFT_RESET_PIN);
  SET_GPIO_MODE(GPIO_TFT_RESET_PIN, 1);
  SET_GPIO(GPIO_TFT_RESET_PIN);
  usleep(120 * 1000);
  CLEAR_GPIO(GPIO_TFT_RESET_PIN);
  usleep(120 * 1000);
  SET_GPIO(GPIO_TFT_RESET_PIN);
  usleep(120 * 1000);
#endif

  // Do the initialization with a very low SPI bus speed, so that it will succeed even if the bus speed chosen by the user is too high.
  spi->clk = 100;
  __sync_synchronize();

  BEGIN_SPI_COMMUNICATION();
  {
    SPI_TRANSFER(0xFD/*Set Command Lock*/, 0x12);
    SPI_TRANSFER(0xFD/*Set Command Lock*/, 0xB1);
    SPI_TRANSFER(0xAE/*Sleep Mode On (Display OFF)*/);

    SPI_TRANSFER(0xB3/*Set Front Clock Divider/Oscillator Frequency*/, 0xF1/*Divide Ratio=1, Oscillator Frequency=0xF*/); // This controls frame rate -> set to fastest
    SPI_TRANSFER(0xCA/*Set Multiplex Ratio*/, 95); // This effectively sets the pixel height of the display, set this to 127 for 128x128 OLED, and to 95 for 128x96 OLED. It looks like even the 128x96 OLED has 128x128 bytes worth of internal memory (for hardware scrolling?)
    SPI_TRANSFER(0xA0/*Set Remap*/, 0x34/*0x04=BGR<->RGB Swap | 0x10=Vertical swap | 0x20=Enable COM split odd even (this makes pixel addressing sane as one'd expect)*/);
    SPI_TRANSFER(0xA1/*Set Display Start Line*/, 0);
    SPI_TRANSFER(0xA2/*Set Display Offset*/, 0);
    SPI_TRANSFER(0xAB/*Set Function Select*/, 0x01/*16bpp colors*/);
    SPI_TRANSFER(0xB5/*Set GPIO0 and GPIO1 pin*/, 0);
    SPI_TRANSFER(0xC1/*Set Contrast Current for Color A,B,C*/, 0xC8, 0x80, 0xC8); // These three seem to be first for red, second for green and third for blue, 0x00-0xFF
    SPI_TRANSFER(0xC7/*Master Contrast Current Control*/, 0x0F); // 0x0F=max contrast, smaller valuers=dimmer and less power consumption

//  Some voltage settings from the spec sheet to try out, although power on defaults seem to work fine as well:
//    SPI_TRANSFER(0xB1/*Set Phase Length*/, 0x32/*Phase1=0, Phase2=4*/);
//    SPI_TRANSFER(0xBE/*Set VCOMH Voltage*/, 0x05);
//    SPI_TRANSFER(0xB4/*Set Segment Low Voltage*/, 0xA0, 0xB5, 0x55);
//    SPI_TRANSFER(0xB6/*Set Second Precharge Period*/, 0x01/*1 DCLK*/);

    SPI_TRANSFER(0xA6/*Set Display Normal*/);
    SPI_TRANSFER(0xAF/*Sleep Mode OFF/Display ON*/);
    ClearScreen();
  }
#ifndef USE_DMA_TRANSFERS // For DMA transfers, keep SPI CS & TA active.
  END_SPI_COMMUNICATION();
#endif

  // And speed up to the desired operation speed finally after init is done.
  usleep(10 * 1000); // Delay a bit before restoring CLK, or otherwise this has been observed to cause the display not init if done back to back after the clear operation above.
  spi->clk = SPI_BUS_CLOCK_DIVISOR;
}

void TurnDisplayOff()
{
#if defined(GPIO_TFT_BACKLIGHT) && defined(BACKLIGHT_CONTROL)
  SET_GPIO_MODE(GPIO_TFT_BACKLIGHT, 0x01); // Set backlight pin to digital 0/1 output mode (0x01) in case it had been PWM controlled
  CLEAR_GPIO(GPIO_TFT_BACKLIGHT); // And turn the backlight off.
#endif
#if 0
  QUEUE_SPI_TRANSFER(0x28/*Display OFF*/);
  QUEUE_SPI_TRANSFER(0x10/*Enter Sleep Mode*/);
  usleep(120*1000); // Sleep off can be sent 120msecs after entering sleep mode the earliest, so synchronously sleep here for that duration to be safe.
#endif
//  printf("Turned display OFF\n");
}

void TurnDisplayOn()
{
#if 0
  QUEUE_SPI_TRANSFER(0x11/*Sleep Out*/);
  usleep(120 * 1000);
  QUEUE_SPI_TRANSFER(0x29/*Display ON*/);
#endif
#if defined(GPIO_TFT_BACKLIGHT) && defined(BACKLIGHT_CONTROL)
  SET_GPIO_MODE(GPIO_TFT_BACKLIGHT, 0x01); // Set backlight pin to digital 0/1 output mode (0x01) in case it had been PWM controlled
  SET_GPIO(GPIO_TFT_BACKLIGHT); // And turn the backlight on.
#endif
//  printf("Turned display ON\n");
}

void DeinitSPIDisplay()
{
  ClearScreen();
}

#endif
