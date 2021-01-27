#pragma once

#include "config.h"

#ifdef MPI3501

// Data specific to the KeDei v6.3 display
#define DISPLAY_SET_CURSOR_X 0x2A001100
#define DISPLAY_SET_CURSOR_Y 0x2B001100
#define DISPLAY_WRITE_PIXELS 0x2C001100

#define DISPLAY_NATIVE_WIDTH 320
#define DISPLAY_NATIVE_HEIGHT 480

// On KeDei v6.3 display, each 16-bit command (of which highest 8 bits are always 0x00) is always prepended with a 16-bit command/data prefix that is either 0x0011 (command) or 0x0015 (data).
#define DISPLAY_SPI_BUS_IS_16BITS_WIDE

// KeDei v6.3 does not behave well if one sends partial commands, but must finish each command or the command does not apply
#define MUST_SEND_FULL_CURSOR_WINDOW

// KeDei v6.3 is a 3-wire SPI display, DC line is not used
#define SPI_3WIRE_PROTOCOL

// KeDei frames all command/data packets with a 16 bit prefix.
#define SPI_3WIRE_DATA_COMMAND_FRAMING_BITS 16

// On KeDei, SPI commands are 32-bit wide, instead of 8-bit or 16-bit.
#define SPI_32BIT_COMMANDS

// SPI drive settings are different compared to most other displays: KeDei SPI hat connects display to SPI channel 1 (channel 0 is for touch controller),
// and Polarity and Phase are reversed. (Chip Select line is idle when high, and bits are clocked on rising edge of the serial clock line)
#define DISPLAY_SPI_DRIVE_SETTINGS (1 | BCM2835_SPI0_CS_CPOL | BCM2835_SPI0_CS_CPHA)

// A peculiarity of KeDei is that it needs the Touch and Display CS lines pumped for each 32-bit word that is written, or otherwise it does not process bytes on the bus. (it does send
// return bytes back on the MISO line though even without this, so it does at least do something even without this, but nothing would show up on the screen if this pumping is not done)
#define CHIP_SELECT_LINE_NEEDS_REFRESHING_EACH_32BITS_WRITTEN

// On KeDei, CS0 line is for touch, and CS1 line is for the LCD
#define DISPLAY_USES_CS1

#ifdef USE_DMA_TRANSFERS
#warning KeDei v6.3 controller does not currently support DMA, rebuild with CMake directive -DUSE_DMA_TRANSFERS=OFF.
#endif

void InitKeDeiV63(void);
#define InitSPIDisplay InitKeDeiV63

#endif
