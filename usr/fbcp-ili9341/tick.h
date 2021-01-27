#pragma once

#ifndef KERNEL_MODULE
#include <inttypes.h>
#include <unistd.h>

// Initialized in spi.cpp along with the rest of the BCM2835 peripheral:
extern volatile uint64_t *systemTimerRegister;
#define tick() (*systemTimerRegister)

#endif


#ifdef NO_THROTTLING
#define usleep(x) ((void)0)
#endif
