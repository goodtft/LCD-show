#include <linux/input.h> // input_event
#include <fcntl.h> // O_RDONLY, O_NONBLOCK
#include <stdio.h> // printf
#include <stdint.h> // uint64_t

#include "config.h"
#include "keyboard.h"
#include "util.h"
#include "tick.h"

#if defined(BACKLIGHT_CONTROL_FROM_KEYBOARD) && defined(TURN_DISPLAY_OFF_AFTER_USECS_OF_INACTIVITY)
#define READ_KEYBOARD_ENABLED
#endif

int key_fd = -1;

void OpenKeyboard()
{
#ifdef READ_KEYBOARD_ENABLED
  key_fd = open(KEYBOARD_INPUT_FILE, O_RDONLY|O_NONBLOCK);
  if (key_fd < 0) printf("Warning: cannot open keyboard input file " KEYBOARD_INPUT_FILE "! Try double checking that it exists, or reconfigure it in keyboard.cpp, or remove line '#define BACKLIGHT_CONTROL_FROM_KEYBOARD' in config.h if you do not want keyboard activity to factor into backlight control.\n");
#endif
}

int ReadKeyboard()
{
#ifdef READ_KEYBOARD_ENABLED
  if (key_fd < 0) return 0;
  struct input_event ev;
  ssize_t bytesRead = -1;
  int numRead = 0;
  do
  {
    bytesRead = read(key_fd, &ev, sizeof(struct input_event));
    if (bytesRead >= sizeof(struct input_event))
    {
      if (ev.type == 1 && ev.code != 0) // key up or down
      {
//        printf("time: %d %d type: %d, code: %d, value: %d\n", ev.time.tv_sec, ev.time.tv_usec, ev.type, ev.code, ev.value);
        ++numRead;
      }
    }
  } while(bytesRead > 0);
  return numRead;
#else
  return 0;
#endif
}

void CloseKeyboard()
{
#ifdef READ_KEYBOARD_ENABLED
  if (key_fd >= 0)
  {
    close(key_fd);
    key_fd = -1;
  }
#endif
}

static uint64_t lastKeyboardPressTime = 0;
static uint64_t lastKeyboardPressCheckTime = 0;

uint64_t TimeSinceLastKeyboardPress(void)
{
#ifdef READ_KEYBOARD_ENABLED
  uint64_t now = tick();
  if (now - lastKeyboardPressCheckTime >= 250000) // ReadKeyboard() takes about 8 usecs on Pi 3B, so 250msecs poll interval should be fine
  {
    lastKeyboardPressCheckTime = now;
    if (ReadKeyboard())
      lastKeyboardPressTime = now;
  }
  return now - lastKeyboardPressTime;
#else
  return 0;
#endif
}
