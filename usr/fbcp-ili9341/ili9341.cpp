#include "config.h"

#if defined(ILI9341) || defined(ILI9340)

#include "spi.h"

#include <memory.h>
#include <stdio.h>

void InitILI9341()
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
  spi->clk = 34;
  __sync_synchronize();

  BEGIN_SPI_COMMUNICATION();
  {
    SPI_TRANSFER(0x01/*Software Reset*/);
    usleep(5*1000);
    SPI_TRANSFER(0x28/*Display OFF*/);
    // The following appear in ILI9341 Data Sheet v1.11 (2011/06/10), but not in older v1.02 (2010/12/06).
    SPI_TRANSFER(0xCB/*Power Control A*/, 0x39/*Reserved*/, 0x2C/*Reserved*/, 0x00/*Reserved*/, 0x34/*REG_VD=1.6V*/, 0x02/*VBC=5.6V*/); // These are the same as power on.
    SPI_TRANSFER(0xCF/*Power Control B*/, 0x00/*Always Zero*/, 0xC1/*Power Control=0,DRV_ena=0,PCEQ=1*/, 0x30/*DC_ena=1*/); // Not sure what the effect is, set to default as per ILI9341 Application Notes v0.6 (2011/03/11) document (which is not apparently same as default at power on).
    SPI_TRANSFER(0xE8/*Driver Timing Control A*/, 0x85, 0x00, 0x78); // Not sure what the effect is, set to default as per ILI9341 Application Notes v0.6 (2011/03/11) document (which is not apparently same as default at power on).
    SPI_TRANSFER(0xEA/*Driver Timing Control B*/, 0x00, 0x00); // Not sure what the effect is, set to default as per ILI9341 Application Notes v0.6 (2011/03/11) document (which is not apparently same as default at power on).
    SPI_TRANSFER(0xED/*Power On Sequence Control*/, 0x64, 0x03, 0x12, 0x81); // Not sure what the effect is, set to default as per ILI9341 Application Notes v0.6 (2011/03/11) document (which is not apparently same as default at power on).
#if ILI9341_UPDATE_FRAMERATE == ILI9341_FRAMERATE_119_HZ // Setting pump ratio if update rate is less than 119 Hz does not look good but produces shimmering in panning motion.
    SPI_TRANSFER(0xF7/*Pump Ratio Control*/, ILI9341_PUMP_CONTROL); 
#endif
    // The following appear also in old ILI9341 Data Sheet v1.02 (2010/12/06).
    SPI_TRANSFER(0xC0/*Power Control 1*/, 0x23/*VRH=4.60V*/); // Set the GVDD level, which is a reference level for the VCOM level and the grayscale voltage level.
    SPI_TRANSFER(0xC1/*Power Control 2*/, 0x10/*AVCC=VCIx2,VGH=VCIx7,VGL=-VCIx4*/); // Sets the factor used in the step-up circuits. To reduce power consumption, set a smaller factor.
    SPI_TRANSFER(0xC5/*VCOM Control 1*/, 0x3e/*VCOMH=4.250V*/, 0x28/*VCOML=-1.500V*/); // Adjusting VCOM 1 and 2 can control display brightness
    SPI_TRANSFER(0xC7/*VCOM Control 2*/, 0x86/*VCOMH=VMH-58,VCOML=VML-58*/);

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
    SPI_TRANSFER(0x36/*MADCTL: Memory Access Control*/, madctl);

#ifdef DISPLAY_INVERT_COLORS
    SPI_TRANSFER(0x21/*Display Inversion ON*/);
#else
    SPI_TRANSFER(0x20/*Display Inversion OFF*/);
#endif
    SPI_TRANSFER(0x3A/*COLMOD: Pixel Format Set*/, 0x55/*DPI=16bits/pixel,DBI=16bits/pixel*/);

    // According to spec sheet, display frame rate in 4-wire SPI "internal clock mode" is computed with the following formula:
    // frameRate = 615000 / [ (pow(2,DIVA) * (320 + VFP + VBP) * RTNA ]
    // where
    // - DIVA is clock division ratio, 0 <= DIVA <= 3; so pow(2,DIVA) is either 1, 2, 4 or 8.
    // - RTNA specifies the number of clocks assigned to each horizontal scanline, and must follow 16 <= RTNA <= 31.
    // - VFP is vertical front porch, number of idle sleep scanlines before refreshing a new frame, 2 <= VFP <= 127.
    // - VBP is vertical back porch, number of idle sleep scanlines after refreshing a new frame, 2 <= VBP <= 127.

    // Max refresh rate then is with DIVA=0, VFP=2, VBP=2 and RTNA=16:
    // maxFrameRate = 615000 / (1 * (320 + 2 + 2) * 16) = 118.63 Hz

    // To get 60fps, set DIVA=0, RTNA=31, VFP=2 and VBP=2:
    // minFrameRate = 615000 / (8 * (320 + 2 + 2) * 31) = 61.23 Hz

    // It seems that in internal clock mode, horizontal front and back porch settings (HFP, BFP) are ignored(?)

    SPI_TRANSFER(0xB1/*Frame Rate Control (In Normal Mode/Full Colors)*/, 0x00/*DIVA=fosc*/, ILI9341_UPDATE_FRAMERATE/*RTNA(Frame Rate)*/);
//    SPI_TRANSFER(0xB5/*Blanking Porch Control*/, 0x02/*VFP, vertical front porch*/, 0x02/*VBP, vertical back porch*/, 0x0A/*HFP, horizontal front porch*/, 0x14/*HBP, horizontal back porch*/); // These are the default values at power on
    SPI_TRANSFER(0xB6/*Display Function Control*/, 0x08/*PTG=Interval Scan,PT=V63/V0/VCOML/VCOMH*/, 0x82/*REV=1(Normally white),ISC(Scan Cycle)=5 frames*/, 0x27/*LCD Driver Lines=320*/);
    SPI_TRANSFER(0xF2/*Enable 3G*/, 0x02/*False*/); // This one is present only in ILI9341 Data Sheet v1.11 (2011/06/10)
    SPI_TRANSFER(0x26/*Gamma Set*/, 0x01/*Gamma curve 1 (G2.2)*/);
    SPI_TRANSFER(0xE0/*Positive Gamma Correction*/, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00);
    SPI_TRANSFER(0xE1/*Negative Gamma Correction*/, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F);
    SPI_TRANSFER(0x11/*Sleep Out*/);
    usleep(120 * 1000);
    SPI_TRANSFER(/*Display ON*/0x29);

#if defined(GPIO_TFT_BACKLIGHT) && defined(BACKLIGHT_CONTROL)
    printf("Setting TFT backlight on at pin %d\n", GPIO_TFT_BACKLIGHT);
    TurnBacklightOn();
#endif

    // Some wonky effects to try out:
//    SPI_TRANSFER(0x20/*Display Inversion OFF*/);
//    SPI_TRANSFER(0x21/*Display Inversion ON*/);
//    SPI_TRANSFER(0x38/*Idle Mode OFF*/);
//    SPI_TRANSFER(0x39/*Idle Mode ON*/); // Idle mode gives a super-saturated high contrast reduced colors mode

    ClearScreen();
  }
#ifndef USE_DMA_TRANSFERS // For DMA transfers, keep SPI CS & TA active.
  END_SPI_COMMUNICATION();
#endif

  // And speed up to the desired operation speed finally after init is done.
  usleep(10 * 1000); // Delay a bit before restoring CLK, or otherwise this has been observed to cause the display not init if done back to back after the clear operation above.
  spi->clk = SPI_BUS_CLOCK_DIVISOR;
}

void TurnBacklightOn()
{
#if defined(GPIO_TFT_BACKLIGHT) && defined(BACKLIGHT_CONTROL)
  SET_GPIO_MODE(GPIO_TFT_BACKLIGHT, 0x01); // Set backlight pin to digital 0/1 output mode (0x01) in case it had been PWM controlled
  SET_GPIO(GPIO_TFT_BACKLIGHT); // And turn the backlight on.
#endif
}

void TurnBacklightOff()
{
#if defined(GPIO_TFT_BACKLIGHT) && defined(BACKLIGHT_CONTROL)
  SET_GPIO_MODE(GPIO_TFT_BACKLIGHT, 0x01); // Set backlight pin to digital 0/1 output mode (0x01) in case it had been PWM controlled
  CLEAR_GPIO(GPIO_TFT_BACKLIGHT); // And turn the backlight off.
#endif
}

void TurnDisplayOff()
{
  TurnBacklightOff();
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
  SPI_TRANSFER(/*Display OFF*/0x28);
  TurnBacklightOff();
}

#endif
