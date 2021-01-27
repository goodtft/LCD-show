#include "config.h"

#ifdef MPI3501

#include "spi.h"

#include <memory.h>
#include <stdio.h>

void ChipSelectHigh()
{
  WAIT_SPI_FINISHED();
  CLEAR_GPIO(GPIO_SPI0_CE0); // Enable Touch
  SET_GPIO(GPIO_SPI0_CE0); // Disable Touch
  __sync_synchronize();
  SET_GPIO(GPIO_SPI0_CE1); // Disable Display
  CLEAR_GPIO(GPIO_SPI0_CE1); // Enable Display
  __sync_synchronize();
}

void InitKeDeiV63()
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

  // For sanity, start with both Chip selects high to ensure that the display will see a high->low enable transition when we start.
  SET_GPIO(GPIO_SPI0_CE0); // Disable Touch
  SET_GPIO(GPIO_SPI0_CE1); // Disable Display
  usleep(1000);

  // Do the initialization with a very low SPI bus speed, so that it will succeed even if the bus speed chosen by the user is too high.
  spi->clk = 34;
  __sync_synchronize();

  BEGIN_SPI_COMMUNICATION();
  {
    CLEAR_GPIO(GPIO_SPI0_CE0); // Enable Touch
    CLEAR_GPIO(GPIO_SPI0_CE1); // Enable Display

    BEGIN_SPI_COMMUNICATION();

    usleep(25*1000);

    SET_GPIO(GPIO_SPI0_CE0); // Disable Touch
    usleep(25*1000);

    SPI_TRANSFER(0x00000000); // This command seems to be Reset
    usleep(120*1000);

    SPI_TRANSFER(0x00000100);
    usleep(50*1000);
    SPI_TRANSFER(0x00001100);
    usleep(60*1000);

    SPI_TRANSFER(0xB9001100, 0x00, 0xFF, 0x00, 0x83, 0x00, 0x57);
    usleep(5*1000);

    SPI_TRANSFER(0xB6001100, 0x00, 0x2C);
    SPI_TRANSFER(0x11001100/*Sleep Out*/);
    usleep(150*1000);

    SPI_TRANSFER(0x3A001100/*Interface Pixel Format*/, 0x00, 0x55);
    SPI_TRANSFER(0xB0001100, 0x00, 0x68);
    SPI_TRANSFER(0xCC001100, 0x00, 0x09);
    SPI_TRANSFER(0xB3001100, 0x00, 0x43, 0x00, 0x00, 0x00, 0x06, 0x00, 0x06);
    SPI_TRANSFER(0xB1001100, 0x00, 0x00, 0x00, 0x15, 0x00, 0x1C, 0x00, 0x1C, 0x00, 0x83, 0x00, 0x44);
    SPI_TRANSFER(0xC0001100, 0x00, 0x24, 0x00, 0x24, 0x00, 0x01, 0x00, 0x3C, 0x00, 0x1E, 0x00, 0x08);
    SPI_TRANSFER(0xB4001100, 0x00, 0x02, 0x00, 0x40, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x2A, 0x00, 0x0D, 0x00, 0x4F);
    SPI_TRANSFER(0xE0001100, 0x00, 0x02, 0x00, 0x08, 0x00, 0x11, 0x00, 0x23, 0x00, 0x2C, 0x00, 0x40, 0x00, 0x4A, 0x00, 0x52, 0x00, 0x48, 0x00, 0x41, 0x00, 0x3C, 0x00, 0x33, 0x00, 0x2E, 0x00, 0x28, 0x00, 0x27, 0x00, 0x1B, 0x00, 0x02, 0x00, 0x08, 0x00, 0x11, 0x00, 0x23, 0x00, 0x2C, 0x00, 0x40, 0x00, 0x4A, 0x00, 0x52, 0x00, 0x48, 0x00, 0x41, 0x00, 0x3C, 0x00, 0x33, 0x00, 0x2E, 0x00, 0x28, 0x00, 0x27, 0x00, 0x1B, 0x00, 0x00, 0x00, 0x01);

#define MADCTL_BGR_PIXEL_ORDER (1<<3)
#define MADCTL_ROW_COLUMN_EXCHANGE (1<<5)
#define MADCTL_COLUMN_ADDRESS_ORDER_SWAP (1<<6)
#define MADCTL_ROW_ADDRESS_ORDER_SWAP (1<<7)
#define MADCTL_ROTATE_180_DEGREES (MADCTL_COLUMN_ADDRESS_ORDER_SWAP | MADCTL_ROW_ADDRESS_ORDER_SWAP)

    uint8_t madctl = 0;
#ifndef DISPLAY_SWAP_BGR
    madctl |= MADCTL_BGR_PIXEL_ORDER;
#endif
#if defined(DISPLAY_FLIP_ORIENTATION_IN_HARDWARE)
    madctl |= MADCTL_ROW_COLUMN_EXCHANGE;
#endif
#ifdef DISPLAY_ROTATE_180_DEGREES
    madctl ^= MADCTL_ROTATE_180_DEGREES;
#endif
    SPI_TRANSFER(0x36001100/*MADCTL: Memory Access Control*/, 0x00, madctl);

    SPI_TRANSFER(0x29001100/*Display ON*/);

    usleep(200*1000);

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
}

void TurnBacklightOn()
{
}

void TurnDisplayOff()
{
}

void TurnDisplayOn()
{
}

void DeinitSPIDisplay()
{
  ClearScreen();
  TurnDisplayOff();
}

#endif
