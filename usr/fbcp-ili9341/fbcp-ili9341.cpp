#include <fcntl.h>
#include <linux/fb.h>
#include <linux/futex.h>
#include <linux/spi/spidev.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <endian.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>
#include <math.h>
#include <signal.h>

#include "config.h"
#include "text.h"
#include "spi.h"
#include "gpu.h"
#include "statistics.h"
#include "tick.h"
#include "display.h"
#include "util.h"
#include "mailbox.h"
#include "diff.h"
#include "mem_alloc.h"
#include "keyboard.h"
#include "low_battery.h"

int CountNumChangedPixels(uint16_t *framebuffer, uint16_t *prevFramebuffer)
{
  int changedPixels = 0;
  for(int y = 0; y < gpuFrameHeight; ++y)
  {
    for(int x = 0; x < gpuFrameWidth; ++x)
      if (framebuffer[x] != prevFramebuffer[x])
        ++changedPixels;

    framebuffer += gpuFramebufferScanlineStrideBytes >> 1;
    prevFramebuffer += gpuFramebufferScanlineStrideBytes >> 1;
  }
  return changedPixels;
}

uint64_t displayContentsLastChanged = 0;
bool displayOff = false;

volatile bool programRunning = true;

const char *SignalToString(int signal)
{
  if (signal == SIGINT) return "SIGINT";
  if (signal == SIGQUIT) return "SIGQUIT";
  if (signal == SIGUSR1) return "SIGUSR1";
  if (signal == SIGUSR2) return "SIGUSR2";
  if (signal == SIGTERM) return "SIGTERM";
  return "?";
}

void MarkProgramQuitting()
{
  programRunning = false;
}

void ProgramInterruptHandler(int signal)
{
  printf("Signal %s(%d) received, quitting\n", SignalToString(signal), signal);
  static int quitHandlerCalled = 0;
  if (++quitHandlerCalled >= 5)
  {
    printf("Ctrl-C handler invoked five times, looks like fbcp-ili9341 is not gracefully quitting - performing a forcible shutdown!\n");
    exit(1);
  }
  MarkProgramQuitting();
  __sync_synchronize();
  // Wake the SPI thread if it was sleeping so that it can gracefully quit
  if (spiTaskMemory)
  {
    __atomic_fetch_add(&spiTaskMemory->queueHead, 1, __ATOMIC_SEQ_CST);
    __atomic_fetch_add(&spiTaskMemory->queueTail, 1, __ATOMIC_SEQ_CST);
    syscall(SYS_futex, &spiTaskMemory->queueTail, FUTEX_WAKE, 1, 0, 0, 0); // Wake the SPI thread if it was sleeping to get new tasks
  }

  // Wake the main thread if it was sleeping for a new frame so that it can gracefully quit
  __atomic_fetch_add(&numNewGpuFrames, 1, __ATOMIC_SEQ_CST);
  syscall(SYS_futex, &numNewGpuFrames, FUTEX_WAKE, 1, 0, 0, 0);
}

int main()
{
  signal(SIGINT, ProgramInterruptHandler);
  signal(SIGQUIT, ProgramInterruptHandler);
  signal(SIGUSR1, ProgramInterruptHandler);
  signal(SIGUSR2, ProgramInterruptHandler);
  signal(SIGTERM, ProgramInterruptHandler);
#ifdef RUN_WITH_REALTIME_THREAD_PRIORITY
  SetRealtimeThreadPriority();
#endif
  OpenMailbox();
  InitSPI();
  displayContentsLastChanged = tick();
  displayOff = false;
  InitLowBatterySystem();

  // Track current SPI display controller write X and Y cursors.
  int spiX = -1;
  int spiY = -1;
  int spiEndX = DISPLAY_WIDTH;

  InitGPU();

  spans = (Span*)Malloc((gpuFrameWidth * gpuFrameHeight / 2) * sizeof(Span), "main() task spans");
  int size = gpuFramebufferSizeBytes;
#ifdef USE_GPU_VSYNC
  // BUG in vc_dispmanx_resource_read_data(!!): If one is capturing a small subrectangle of a large screen resource rectangle, the destination pointer 
  // is in vc_dispmanx_resource_read_data() incorrectly still taken to point to the top-left corner of the large screen resource, instead of the top-left
  // corner of the subrectangle to capture. Therefore do dirty pointer arithmetic to adjust for this. To make this safe, videoCoreFramebuffer is allocated
  // double its needed size so that this adjusted pointer does not reference outside allocated memory (if it did, vc_dispmanx_resource_read_data() was seen
  // to randomly fail and then subsequently hang if called a second time)
  size *= 2;
#endif
  uint16_t *framebuffer[2] = { (uint16_t *)Malloc(size, "main() framebuffer0"), (uint16_t *)Malloc(gpuFramebufferSizeBytes, "main() framebuffer1") };
  memset(framebuffer[0], 0, size); // Doublebuffer received GPU memory contents, first buffer contains current GPU memory,
  memset(framebuffer[1], 0, gpuFramebufferSizeBytes); // second buffer contains whatever the display is currently showing. This allows diffing pixels between the two.
#ifdef USE_GPU_VSYNC
  // Due to the above bug. In USE_GPU_VSYNC mode, we directly snapshot to framebuffer[0], so it has to be prepared specially to work around the
  // dispmanx bug.
  framebuffer[0] += (gpuFramebufferSizeBytes>>1);
#endif

  uint32_t curFrameEnd = spiTaskMemory->queueTail;
  uint32_t prevFrameEnd = spiTaskMemory->queueTail;

  bool prevFrameWasInterlacedUpdate = false;
  bool interlacedUpdate = false; // True if the previous update we did was an interlaced half field update.
  int frameParity = 0; // For interlaced frame updates, this is either 0 or 1 to denote evens or odds.
  OpenKeyboard();
  printf("All initialized, now running main loop...\n");
  while(programRunning)
  {
    prevFrameWasInterlacedUpdate = interlacedUpdate;

    // If last update was interlaced, it means we still have half of the image pending to be updated. In such a case,
    // sleep only until when we expect the next new frame of data to appear, and then continue independent of whether
    // a new frame was produced or not - if not, then we will submit the rest of the unsubmitted fields. If yes, then
    // the half fields of the new frame will be sent (or full, if the new frame has very little content)
    if (prevFrameWasInterlacedUpdate)
    {
#ifdef THROTTLE_INTERLACING
      timespec timeout = {};
      timeout.tv_nsec = 1000 * MIN(1000000, MAX(1, 750/*0.75ms extra sleep so we know we should likely sleep long enough to see the next frame*/ + PredictNextFrameArrivalTime() - tick()));
      if (programRunning) syscall(SYS_futex, &numNewGpuFrames, FUTEX_WAIT, 0, &timeout, 0, 0); // Start sleeping until we get new tasks
#endif
      // If THROTTLE_INTERLACING is not defined, we'll fall right through and immediately submit the rest of the remaining content on screen to attempt to minimize the visual
      // observable effect of interlacing, although at the expense of smooth animation (falling through here causes jitter)
    }
    else
    {
      uint64_t waitStart = tick();
      while(__atomic_load_n(&numNewGpuFrames, __ATOMIC_SEQ_CST) == 0)
      {
#if defined(BACKLIGHT_CONTROL) && defined(TURN_DISPLAY_OFF_AFTER_USECS_OF_INACTIVITY)
        if (!displayOff && tick() - waitStart > TURN_DISPLAY_OFF_AFTER_USECS_OF_INACTIVITY)
        {
          TurnDisplayOff();
          displayOff = true;
        }

        if (!displayOff)
        {
          timespec timeout = {};
          timeout.tv_sec = ((uint64_t)TURN_DISPLAY_OFF_AFTER_USECS_OF_INACTIVITY * 1000) / 1000000000;
          timeout.tv_nsec = ((uint64_t)TURN_DISPLAY_OFF_AFTER_USECS_OF_INACTIVITY * 1000) % 1000000000;
          if (programRunning) syscall(SYS_futex, &numNewGpuFrames, FUTEX_WAIT, 0, &timeout, 0, 0); // Sleep until the next frame arrives
        }
        else
#endif
          if (programRunning) syscall(SYS_futex, &numNewGpuFrames, FUTEX_WAIT, 0, 0, 0, 0); // Sleep until the next frame arrives
      }
    }

    bool spiThreadWasWorkingHardBefore = false;

    // At all times keep at most two rendered frames in the SPI task queue pending to be displayed. Only proceed to submit a new frame
    // once the older of those has been displayed.
    bool once = true;
    while ((spiTaskMemory->queueTail + SPI_QUEUE_SIZE - spiTaskMemory->queueHead) % SPI_QUEUE_SIZE > (spiTaskMemory->queueTail + SPI_QUEUE_SIZE - prevFrameEnd) % SPI_QUEUE_SIZE)
    {
      if (spiTaskMemory->spiBytesQueued > 10000)
        spiThreadWasWorkingHardBefore = true; // SPI thread had too much work in queue atm (2 full frames)

      // Peek at the SPI thread's workload and throttle a bit if it has got a lot of work still to do.
      double usecsUntilSpiQueueEmpty = spiTaskMemory->spiBytesQueued*spiUsecsPerByte;
      if (usecsUntilSpiQueueEmpty > 0)
      {
        uint32_t bytesInQueueBefore = spiTaskMemory->spiBytesQueued;
        uint32_t sleepUsecs = (uint32_t)(usecsUntilSpiQueueEmpty*0.4);
#ifdef STATISTICS
        uint64_t t0 = tick();
#endif
        if (sleepUsecs > 1000) usleep(500);

#ifdef STATISTICS
        uint64_t t1 = tick();
        uint32_t bytesInQueueAfter = spiTaskMemory->spiBytesQueued;
        bool starved = (spiTaskMemory->queueHead == spiTaskMemory->queueTail);
        if (starved) spiThreadWasWorkingHardBefore = false;

/*
        if (once && starved)
        {
          printf("Had %u bytes in queue, asked to sleep for %u usecs, got %u usecs sleep, afterwards %u bytes in queue. (got %.2f%% work done)%s\n",
            bytesInQueueBefore, sleepUsecs, (uint32_t)(t1 - t0), bytesInQueueAfter, (bytesInQueueBefore-bytesInQueueAfter)*100.0/bytesInQueueBefore,
            starved ? "  SLEPT TOO LONG, SPI THREAD STARVED" : "");
          once = false;
        }
*/
#endif
      }
    }

    int expiredFrames = 0;
    uint64_t now = tick();
    while(expiredFrames < frameTimeHistorySize && now - frameTimeHistory[expiredFrames].time >= FRAMERATE_HISTORY_LENGTH) ++expiredFrames;
    if (expiredFrames > 0)
    {
      frameTimeHistorySize -= expiredFrames;
      for(int i = 0; i < frameTimeHistorySize; ++i) frameTimeHistory[i] = frameTimeHistory[i+expiredFrames];
    }

#ifdef STATISTICS
    int expiredSkippedFrames = 0;
    while(expiredSkippedFrames < frameSkipTimeHistorySize && now - frameSkipTimeHistory[expiredSkippedFrames] >= 1000000/*FRAMERATE_HISTORY_LENGTH*/) ++expiredSkippedFrames;
    if (expiredSkippedFrames > 0)
    {
      frameSkipTimeHistorySize -= expiredSkippedFrames;
      for(int i = 0; i < frameSkipTimeHistorySize; ++i) frameSkipTimeHistory[i] = frameSkipTimeHistory[i+expiredSkippedFrames];
    }
#endif

    int numNewFrames = __atomic_load_n(&numNewGpuFrames, __ATOMIC_SEQ_CST);
    bool gotNewFramebuffer = (numNewFrames > 0);
    bool framebufferHasNewChangedPixels = true;
    uint64_t frameObtainedTime;
    if (gotNewFramebuffer)
    {
#ifdef USE_GPU_VSYNC
      // TODO: Hardcoded vsync interval to 60 for now. Would be better to compute yet another histogram of the vsync arrival times, if vsync is not set to 60hz.
      // N.B. copying directly to videoCoreFramebuffer[1] that may be directly accessed by the main thread, so this could
      // produce a visible tear between two adjacent frames, but since we don't have vsync anyways, currently not caring too much.

      frameObtainedTime = tick();
      uint64_t framePollingStartTime = frameObtainedTime;

#if defined(SAVE_BATTERY_BY_PREDICTING_FRAME_ARRIVAL_TIMES) || defined(SAVE_BATTERY_BY_SLEEPING_WHEN_IDLE)
    uint64_t nextFrameArrivalTime = PredictNextFrameArrivalTime();
    int64_t timeToSleep = nextFrameArrivalTime - tick();
    if (timeToSleep > 0)
      usleep(timeToSleep);
#endif

      framebufferHasNewChangedPixels = SnapshotFramebuffer(framebuffer[0]);
#else
      memcpy(framebuffer[0], videoCoreFramebuffer[1], gpuFramebufferSizeBytes);
#endif

      PollLowBattery();

#ifdef STATISTICS
      uint64_t now = tick();
      for(int i = 0; i < numNewFrames - 1 && frameSkipTimeHistorySize < FRAMERATE_HISTORY_LENGTH; ++i)
        frameSkipTimeHistory[frameSkipTimeHistorySize++] = now;
#endif
      __atomic_fetch_sub(&numNewGpuFrames, numNewFrames, __ATOMIC_SEQ_CST);

      DrawStatisticsOverlay(framebuffer[0]);
      DrawLowBatteryIcon(framebuffer[0]);

#ifdef USE_GPU_VSYNC

#ifdef STATISTICS
      uint64_t completelyUnnecessaryTimeWastedPollingGPUStart = tick();
#endif

      // DispmanX PROBLEM! When latching onto the vsync signal, the DispmanX API sends the signal at arbitrary phase with respect to the application actually producing its frames.
      // Therefore even while we do get a smooth 16.666.. msec interval vsync signal, we have no idea whether the application has actually produced a new frame at that time. Therefore
      // we must keep polling for frames until we find one that it has produced.
#ifdef SELF_SYNCHRONIZE_TO_GPU_VSYNC_PRODUCED_NEW_FRAMES
      framebufferHasNewChangedPixels = framebufferHasNewChangedPixels && IsNewFramebuffer(framebuffer[0], framebuffer[1]);
      uint64_t timeToGiveUpThereIsNotGoingToBeANewFrame = framePollingStartTime + 1000000/TARGET_FRAME_RATE/2;
      while(!framebufferHasNewChangedPixels && tick() < timeToGiveUpThereIsNotGoingToBeANewFrame)
      {
        usleep(2000);
        frameObtainedTime = tick();
        framebufferHasNewChangedPixels = SnapshotFramebuffer(framebuffer[0]);
        DrawStatisticsOverlay(framebuffer[0]);
        DrawLowBatteryIcon(framebuffer[0]);
        framebufferHasNewChangedPixels = framebufferHasNewChangedPixels && IsNewFramebuffer(framebuffer[0], framebuffer[1]);
      }
#else
      framebufferHasNewChangedPixels = true;
#endif

      numNewFrames = __atomic_load_n(&numNewGpuFrames, __ATOMIC_SEQ_CST);
      __atomic_fetch_sub(&numNewGpuFrames, numNewFrames, __ATOMIC_SEQ_CST);

#ifdef STATISTICS
      now = tick();
      for(int i = 0; i < numNewFrames - 1 && frameSkipTimeHistorySize < FRAMERATE_HISTORY_LENGTH; ++i)
        frameSkipTimeHistory[frameSkipTimeHistorySize++] = now;

      uint64_t completelyUnnecessaryTimeWastedPollingGPUStop = tick();
      __atomic_fetch_add(&timeWastedPollingGPU, completelyUnnecessaryTimeWastedPollingGPUStop-completelyUnnecessaryTimeWastedPollingGPUStart, __ATOMIC_RELAXED);
#endif

#else // !USE_GPU_VSYNC
      if (!displayOff)
        RefreshStatisticsOverlayText();
#endif
    }

    // If too many pixels have changed on screen, drop adaptively to interlaced updating to keep up the frame rate.
    double inputDataFps = 1000000.0 / EstimateFrameRateInterval();
    double desiredTargetFps = MAX(1, MIN(inputDataFps, TARGET_FRAME_RATE));
#ifdef SINGLE_CORE_BOARD
    const double timesliceToUseForScreenUpdates = 250000;
#elif defined(ILI9486) || defined(ILI9486L) ||defined(HX8357D)
    const double timesliceToUseForScreenUpdates = 750000;
#else
    const double timesliceToUseForScreenUpdates = 1500000;
#endif
    const double tooMuchToUpdateUsecs = timesliceToUseForScreenUpdates / desiredTargetFps; // If updating the current and new frame takes too many frames worth of allotted time, drop to interlacing.

#if !defined(NO_INTERLACING) || (defined(BACKLIGHT_CONTROL) && defined(TURN_DISPLAY_OFF_AFTER_USECS_OF_INACTIVITY))
    int numChangedPixels = framebufferHasNewChangedPixels ? CountNumChangedPixels(framebuffer[0], framebuffer[1]) : 0;
#endif

#ifdef NO_INTERLACING
    interlacedUpdate = false;
#elif defined(ALWAYS_INTERLACING)
    interlacedUpdate = (numChangedPixels > 0);
#else
    uint32_t bytesToSend = numChangedPixels * SPI_BYTESPERPIXEL + (DISPLAY_DRAWABLE_HEIGHT<<1);
    interlacedUpdate = ((bytesToSend + spiTaskMemory->spiBytesQueued) * spiUsecsPerByte > tooMuchToUpdateUsecs); // Decide whether to do interlacedUpdate - only updates half of the screen
#endif

    if (interlacedUpdate) frameParity = 1-frameParity; // Swap even-odd fields every second time we do an interlaced update (progressive updates ignore field order)
    int bytesTransferred = 0;
    Span *head = 0;

#if defined(ALL_TASKS_SHOULD_DMA) && defined(UPDATE_FRAMES_WITHOUT_DIFFING)
    NoDiffChangedRectangle(head);
#elif defined(ALL_TASKS_SHOULD_DMA) && defined(UPDATE_FRAMES_IN_SINGLE_RECTANGULAR_DIFF)
    DiffFramebuffersToSingleChangedRectangle(framebuffer[0], framebuffer[1], head);
#else
    // Collect all spans in this image
    if (framebufferHasNewChangedPixels || prevFrameWasInterlacedUpdate)
    {
      // If possible, utilize a faster 4-wide pixel diffing method
#ifdef FAST_BUT_COARSE_PIXEL_DIFF
      if (gpuFrameWidth % 4 == 0 && gpuFramebufferScanlineStrideBytes % 8 == 0)
        DiffFramebuffersToScanlineSpansFastAndCoarse4Wide(framebuffer[0], framebuffer[1], interlacedUpdate, frameParity, head);
      else
#endif
        DiffFramebuffersToScanlineSpansExact(framebuffer[0], framebuffer[1], interlacedUpdate, frameParity, head); // If disabled, or framebuffer width is not compatible, use the exact method
    }

    // Merge spans together on adjacent scanlines - works only if doing a progressive update
    if (!interlacedUpdate)
      MergeScanlineSpanList(head);
#endif

#ifdef USE_GPU_VSYNC
    if (head) // do we have a new frame?
    {
      // If using vsync, this main thread is responsible for maintaining the frame histogram. If not using vsync,
      // but instead are using a dedicated GPU thread, then that dedicated thread maintains the frame histogram,
      // in which case this is not needed.
      AddHistogramSample(frameObtainedTime);

      // We got a new frame, so update contents of the statistics overlay as well
      if (!displayOff)
        RefreshStatisticsOverlayText();
    }
#endif

    // Submit spans
    if (!displayOff)
    for(Span *i = head; i; i = i->next)
    {
#ifdef ALIGN_TASKS_FOR_DMA_TRANSFERS
      // DMA transfers smaller than 4 bytes are causing trouble, so in order to ensure smooth DMA operation,
      // make sure each message is at least 4 bytes in size, hence one pixel spans are forbidden:
      if (i->size == 1)
      {
        if (i->endX < DISPLAY_DRAWABLE_WIDTH) { ++i->endX; ++i->lastScanEndX; }
        else --i->x;
        ++i->size;
      }
#endif
      // Update the write cursor if needed
#ifndef DISPLAY_WRITE_PIXELS_CMD_DOES_NOT_RESET_WRITE_CURSOR
      if (spiY != i->y)
#endif
      {
#if defined(MUST_SEND_FULL_CURSOR_WINDOW) || defined(ALIGN_TASKS_FOR_DMA_TRANSFERS)
        QUEUE_SET_WRITE_WINDOW_TASK(DISPLAY_SET_CURSOR_Y, displayYOffset + i->y, displayYOffset + gpuFrameHeight - 1);
#else
        QUEUE_MOVE_CURSOR_TASK(DISPLAY_SET_CURSOR_Y, displayYOffset + i->y);
#endif
        IN_SINGLE_THREADED_MODE_RUN_TASK();
        spiY = i->y;
      }

      if (i->endY > i->y + 1 && (spiX != i->x || spiEndX != i->endX)) // Multiline span?
      {
        QUEUE_SET_WRITE_WINDOW_TASK(DISPLAY_SET_CURSOR_X, displayXOffset + i->x, displayXOffset + i->endX - 1);
        IN_SINGLE_THREADED_MODE_RUN_TASK();
        spiX = i->x;
        spiEndX = i->endX;
      }
      else // Singleline span
      {
#ifdef ALIGN_TASKS_FOR_DMA_TRANSFERS
        if (spiX != i->x || spiEndX < i->endX)
        {
          QUEUE_SET_WRITE_WINDOW_TASK(DISPLAY_SET_CURSOR_X, displayXOffset + i->x, displayXOffset + gpuFrameWidth - 1);
          IN_SINGLE_THREADED_MODE_RUN_TASK();
          spiX = i->x;
          spiEndX = gpuFrameWidth;
        }
#else
        if (spiEndX < i->endX) // Need to push the X end window?
        {
          // We are doing a single line span and need to increase the X window. If possible,
          // peek ahead to cater to the next multiline span update if that will be compatible.
          int nextEndX = gpuFrameWidth;
          for(Span *j = i->next; j; j = j->next)
            if (j->endY > j->y+1)
            {
              if (j->endX >= i->endX) nextEndX = j->endX;
              break;
            }
          QUEUE_SET_WRITE_WINDOW_TASK(DISPLAY_SET_CURSOR_X, displayXOffset + i->x, displayXOffset + nextEndX - 1);
          IN_SINGLE_THREADED_MODE_RUN_TASK();
          spiX = i->x;
          spiEndX = nextEndX;
        }
        else
#ifndef DISPLAY_WRITE_PIXELS_CMD_DOES_NOT_RESET_WRITE_CURSOR
        if (spiX != i->x)
#endif
        {
#ifdef MUST_SEND_FULL_CURSOR_WINDOW
          QUEUE_SET_WRITE_WINDOW_TASK(DISPLAY_SET_CURSOR_X, displayXOffset + i->x, displayXOffset + spiEndX - 1);
#else
          QUEUE_MOVE_CURSOR_TASK(DISPLAY_SET_CURSOR_X, displayXOffset + i->x);
#endif
          IN_SINGLE_THREADED_MODE_RUN_TASK();
          spiX = i->x;
        }
#endif
      }

      // Submit the span pixels
      SPITask *task = AllocTask(i->size*SPI_BYTESPERPIXEL);
      task->cmd = DISPLAY_WRITE_PIXELS;

      bytesTransferred += task->PayloadSize()+1;
      uint16_t *scanline = framebuffer[0] + i->y * (gpuFramebufferScanlineStrideBytes>>1);
      uint16_t *prevScanline = framebuffer[1] + i->y * (gpuFramebufferScanlineStrideBytes>>1);

#ifdef OFFLOAD_PIXEL_COPY_TO_DMA_CPP
      // If running a singlethreaded build without a separate SPI thread, we can offload the whole flow of the pixel data out to the code in the dma.cpp backend,
      // which does the pixel task handoff out to DMA in inline assembly. This is done mainly to save an extra memcpy() when passing data off from GPU to SPI,
      // since in singlethreaded mode, snapshotting GPU and sending data to SPI is done sequentially in this main loop.
      // In multithreaded builds, this approach cannot be used, since after we snapshot a frame, we need to send it off to SPI thread to process, and make a copy
      // anways to ensure it does not get overwritten.
      task->fb = (uint8_t*)(scanline + i->x);
      task->prevFb = (uint8_t*)(prevScanline + i->x);
      task->width = i->endX - i->x;
#else
      uint16_t *data = (uint16_t*)task->data;
      for(int y = i->y; y < i->endY; ++y, scanline += gpuFramebufferScanlineStrideBytes>>1, prevScanline += gpuFramebufferScanlineStrideBytes>>1)
      {
        int endX = (y + 1 == i->endY) ? i->lastScanEndX : i->endX;
        int x = i->x;
#ifdef DISPLAY_COLOR_FORMAT_R6X2G6X2B6X2
        // Convert from R5G6B5 to R6X2G6X2B6X2 on the fly
        while(x < endX)
        {
          uint16_t pixel = scanline[x++];
          uint16_t r = (pixel >> 8) & 0xF8;
          uint16_t g = (pixel >> 3) & 0xFC;
          uint16_t b = (pixel << 3) & 0xF8;
          ((uint8_t*)data)[0] = r | (r >> 5); // On red and blue color channels, need to expand 5 bits to 6 bits. Do that by duplicating the highest bit as lowest bit.
          ((uint8_t*)data)[1] = g;
          ((uint8_t*)data)[2] = b | (b >> 5);
          data = (uint16_t*)((uintptr_t)data + 3);
        }
#else
        while(x < endX && (x&1)) *data++ = __builtin_bswap16(scanline[x++]);
        while(x < (endX&~1U))
        {
          uint32_t u = *(uint32_t*)(scanline+x);
          *(uint32_t*)data = ((u & 0xFF00FF00U) >> 8) | ((u & 0x00FF00FFU) << 8);
          data += 2;
          x += 2;
        }
        while(x < endX) *data++ = __builtin_bswap16(scanline[x++]);
#endif
#if !(defined(ALL_TASKS_SHOULD_DMA) && defined(UPDATE_FRAMES_WITHOUT_DIFFING)) // If not diffing, no need to maintain prev frame.
        memcpy(prevScanline+i->x, scanline+i->x, (endX - i->x)*FRAMEBUFFER_BYTESPERPIXEL);
#endif
      }
#endif
      CommitTask(task);
      IN_SINGLE_THREADED_MODE_RUN_TASK();
    }

#ifdef KERNEL_MODULE_CLIENT
    // Wake the kernel module up to run tasks. TODO: This might not be best placed here, we could pre-empt
    // to start running tasks already half-way during task submission above.
    if (spiTaskMemory->queueHead != spiTaskMemory->queueTail && !(spi->cs & BCM2835_SPI0_CS_TA))
      spi->cs |= BCM2835_SPI0_CS_TA;
#endif

    // Remember where in the command queue this frame ends, to keep track of the SPI thread's progress over it
    if (bytesTransferred > 0)
    {
      prevFrameEnd = curFrameEnd;
      curFrameEnd = spiTaskMemory->queueTail;
    }

#if defined(BACKLIGHT_CONTROL) && defined(TURN_DISPLAY_OFF_AFTER_USECS_OF_INACTIVITY)
    double percentageOfScreenChanged = (double)numChangedPixels/(DISPLAY_DRAWABLE_WIDTH*DISPLAY_DRAWABLE_HEIGHT);
    bool displayIsActive = percentageOfScreenChanged > DISPLAY_CONSIDERED_INACTIVE_PERCENTAGE;
    if (displayIsActive)
      displayContentsLastChanged = tick();

    bool keyboardIsActive = TimeSinceLastKeyboardPress() < TURN_DISPLAY_OFF_AFTER_USECS_OF_INACTIVITY;
    if (displayIsActive || keyboardIsActive)
    {
      if (displayOff)
      {
        TurnDisplayOn();
        displayOff = false;
      }
    }
    else if (!displayOff && tick() - displayContentsLastChanged > TURN_DISPLAY_OFF_AFTER_USECS_OF_INACTIVITY)
    {
      TurnDisplayOff();
      displayOff = true;
    }
#endif

#ifdef STATISTICS
    if (bytesTransferred > 0)
    {
      if (frameTimeHistorySize < FRAME_HISTORY_MAX_SIZE)
      {
        frameTimeHistory[frameTimeHistorySize].interlaced = interlacedUpdate || prevFrameWasInterlacedUpdate;
        frameTimeHistory[frameTimeHistorySize++].time = tick();
      }
      AddFrameCompletionTimeMarker();
    }
    statsBytesTransferred += bytesTransferred;
#endif
  }

  DeinitGPU();
  DeinitSPI();
  CloseMailbox();
  CloseKeyboard();
  printf("Quit.\n");
}
