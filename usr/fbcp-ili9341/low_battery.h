#pragma once

#include <inttypes.h>

// All functions here are no-op when LOW_BATTERY_PIN is undef so they can be
// called unconditionnaly.

// This functions must be called during the startup of the program to initialize
// internal data related to rendering the low battery icon.
void InitLowBatterySystem();

// Polls and saves the state of the battery. No-op if the function was called
// less than LOW_BATTERY_POLLING_INTERVAL tick() ago.
void PollLowBattery();

// Draws a low battery icon on the given framebuffer if the last call to
// pollLowBattery found a low battery state.
void DrawLowBatteryIcon(uint16_t *framebuffer);

