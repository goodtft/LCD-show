#pragma once

#ifdef MZ61581

// SPI_BUS_CLOCK_DIVISOR specifies how fast to communicate the SPI bus at. Possible values are 4, 6, 8, 10, 12, ... Smaller
// values are faster.

// The following bus speed have been tested on Tontec 3.5" display with marking "MZ61581-PI-EXT 2016.1.28" on the back (on a Pi 3B+):

// core_freq=280: CDIV=2, results in 140.00MHz, works
// core_freq=281: CDIV=2, results in 140.50MHz, works, but oddly there is a certain shade of brown color on the ground of OpenTyrian that then starts flickering faintly red - everything else seemed fine. (At 142.5MHz very noticeable)

// Data specific to the MZ61581 controller
#define DISPLAY_SET_CURSOR_X 0x2A
#define DISPLAY_SET_CURSOR_Y 0x2B
#define DISPLAY_WRITE_PIXELS 0x2C

#define DISPLAY_NATIVE_WIDTH 320
#define DISPLAY_NATIVE_HEIGHT 480

#ifdef TONTEC_MZ61581
#include "tontec_35_mz61581.h"
#endif

#define InitSPIDisplay InitMZ61581

void InitMZ61581(void);

void TurnDisplayOn(void);
void TurnDisplayOff(void);

#endif
