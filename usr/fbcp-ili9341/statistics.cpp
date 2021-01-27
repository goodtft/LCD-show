#include "config.h"

#include "statistics.h"

#ifdef STATISTICS

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <pthread.h>
#include <syslog.h>

#include "tick.h"
#include "text.h"
#include "spi.h"
#include "util.h"
#include "mailbox.h"
#include "mem_alloc.h"
#include "dma.h"

volatile uint64_t timeWastedPollingGPU = 0;
volatile float statsSpiBusSpeed = 0;
volatile int statsBcmCoreSpeed = 0;
volatile int statsCpuFrequency = 0;
volatile double statsCpuTemperature = 0;
double spiThreadUtilizationRate;
double spiBusDataRate;
int statsGpuPollingWasted = 0;
uint64_t statsBytesTransferred = 0;

int frameSkipTimeHistorySize = 0;
uint64_t frameSkipTimeHistory[FRAME_HISTORY_MAX_SIZE] = {};

#ifdef FRAME_COMPLETION_TIME_STATISTICS

#define FRAME_COMPLETION_HISTORY_MAX_SIZE 480
uint64_t frameCompletionTimeHistory[FRAME_COMPLETION_HISTORY_MAX_SIZE] = {};
int frameCompletionTimeHistorySize = 0;

int statsFrameIntervalsY[FRAME_COMPLETION_HISTORY_MAX_SIZE] = {};
int statsFrameIntervalsSize = 0;
int statsTargetFrameRateY = 0;
int statsAvgFrameRateIntervalY = 0;

void AddFrameCompletionTimeMarker()
{
  for(int i = frameCompletionTimeHistorySize; i >= 1; --i)
    frameCompletionTimeHistory[i] = frameCompletionTimeHistory[i-1];
  frameCompletionTimeHistory[0] = tick();
  if (frameCompletionTimeHistorySize+1 < FRAME_COMPLETION_HISTORY_MAX_SIZE)
    ++frameCompletionTimeHistorySize;
}
#else
void AddFrameCompletionTimeMarker() {}
#endif

char dmaChannelsText[32] = {};
char fpsText[32] = {};
char spiUsagePercentageText[32] = {};
char spiBusDataRateText[32] = {};
uint16_t spiUsageColor = 0, fpsColor = 0;
char statsFrameSkipText[32] = {};
char spiSpeedText[32] = {};
char spiSpeedText2[32] = {};
char cpuTemperatureText[32] = {};
uint16_t cpuTemperatureColor = 0;
char gpuPollingWastedText[32] = {};
uint16_t gpuPollingWastedColor = 0;

char cpuMemoryUsedText[32] = {};
char gpuMemoryUsedText[32] = {};

uint64_t statsLastPrint = 0;

void UpdateStatisticsNumbers()
{
  // BCM core and SPI bus speed
  int freq = (int)MailboxRet2(0x00030002/*Get Clock Rate*/, 0x4/*CORE*/);
  statsBcmCoreSpeed = freq/1000000;
  statsSpiBusSpeed = (float)freq/(1000000*spi->clk);

  // CPU temperature
  statsCpuTemperature = MailboxRet2(0x00030006/*Get Temperature*/, 0)/1000.0;

  // Raspberry pi main CPU speed
  statsCpuFrequency = (int)MailboxRet2(0x00030002/*Get Clock Rate*/, 0x3/*ARM*/) / 1000000;
}

void DrawStatisticsOverlay(uint16_t *framebuffer)
{
  DrawText(framebuffer, gpuFrameWidth, gpuFramebufferScanlineStrideBytes, gpuFrameHeight, fpsText, 1, 1, fpsColor, 0);
  DrawText(framebuffer, gpuFrameWidth, gpuFramebufferScanlineStrideBytes, gpuFrameHeight, statsFrameSkipText, strlen(fpsText)*6, 1, RGB565(31,0,0), 0);

#if DISPLAY_DRAWABLE_WIDTH > 130
#ifdef USE_DMA_TRANSFERS
  DrawText(framebuffer, gpuFrameWidth, gpuFramebufferScanlineStrideBytes, gpuFrameHeight, dmaChannelsText, 1, 10, RGB565(31, 44, 8), 0);
#endif
#ifdef USE_SPI_THREAD
  DrawText(framebuffer, gpuFrameWidth, gpuFramebufferScanlineStrideBytes, gpuFrameHeight, spiUsagePercentageText, 75, 10, spiUsageColor, 0);
#endif
  DrawText(framebuffer, gpuFrameWidth, gpuFramebufferScanlineStrideBytes, gpuFrameHeight, spiBusDataRateText, 60, 1, 0xFFFF, 0);
#endif

#if DISPLAY_DRAWABLE_WIDTH > 180
  DrawText(framebuffer, gpuFrameWidth, gpuFramebufferScanlineStrideBytes, gpuFrameHeight, spiSpeedText, 120, 1, RGB565(31,14,20), 0);
  DrawText(framebuffer, gpuFrameWidth, gpuFramebufferScanlineStrideBytes, gpuFrameHeight, spiSpeedText2, 120, 10, RGB565(10,24,31), 0);
  DrawText(framebuffer, gpuFrameWidth, gpuFramebufferScanlineStrideBytes, gpuFrameHeight, cpuTemperatureText, 190, 1, cpuTemperatureColor, 0);
  DrawText(framebuffer, gpuFrameWidth, gpuFramebufferScanlineStrideBytes, gpuFrameHeight, gpuPollingWastedText, 222, 1, gpuPollingWastedColor, 0);
#endif

#if (defined(DISPLAY_FLIP_ORIENTATION_IN_SOFTWARE) && DISPLAY_DRAWABLE_HEIGHT >= 290) || (!defined(DISPLAY_FLIP_ORIENTATION_IN_SOFTWARE) && DISPLAY_DRAWABLE_WIDTH >= 290)
  DrawText(framebuffer, gpuFrameWidth, gpuFramebufferScanlineStrideBytes, gpuFrameHeight, cpuMemoryUsedText, 250, 1, RGB565(31,50,21), 0);
  DrawText(framebuffer, gpuFrameWidth, gpuFramebufferScanlineStrideBytes, gpuFrameHeight, gpuMemoryUsedText, 250, 10, RGB565(31,50,31), 0);
#endif

#ifdef FRAME_COMPLETION_TIME_STATISTICS

#ifdef DISPLAY_FLIP_ORIENTATION_IN_SOFTWARE
#define FRAMERATE_GRAPH_WIDTH gpuFrameHeight
#define FRAMERATE_GRAPH_MIN_Y 20
#define FRAMERATE_GRAPH_MAX_Y (gpuFrameWidth - 10)
#define AT(x,y) ((x)*(gpuFramebufferScanlineStrideBytes>>1)+(y))
#else
#define FRAMERATE_GRAPH_WIDTH gpuFrameWidth
#define FRAMERATE_GRAPH_MIN_Y 20
#define FRAMERATE_GRAPH_MAX_Y (gpuFrameHeight - 10)
#define AT(x,y) ((y)*(gpuFramebufferScanlineStrideBytes>>1)+(x))
#endif
  for(int i = 0; i < MIN(statsFrameIntervalsSize, FRAMERATE_GRAPH_WIDTH); ++i)
  {
    int x = FRAMERATE_GRAPH_WIDTH-1-i;
    int y = statsFrameIntervalsY[i];
    framebuffer[AT(x, FRAMERATE_GRAPH_MIN_Y)] = RGB565(31,0,0);
    framebuffer[AT(x, FRAMERATE_GRAPH_MIN_Y+1)] = RGB565(0,0,0);
    framebuffer[AT(x, statsTargetFrameRateY-1)] = RGB565(0,0,0);
    framebuffer[AT(x, statsTargetFrameRateY)] = RGB565(0,63,0);
    framebuffer[AT(x, statsTargetFrameRateY+1)] = RGB565(0,0,0);
    framebuffer[AT(x, statsAvgFrameRateIntervalY-1)] = RGB565(0,0,0);
    framebuffer[AT(x, statsAvgFrameRateIntervalY)] = RGB565(29,50,7);
    framebuffer[AT(x, statsAvgFrameRateIntervalY+1)] = RGB565(0,0,0);
    framebuffer[AT(x, y-3)] = RGB565(0,0,0);
    framebuffer[AT(x, y-2)] = RGB565(0,0,0);
    framebuffer[AT(x, y-1)] = RGB565(5,11,5);
    framebuffer[AT(x, y)] = RGB565(31,63,31);
    framebuffer[AT(x, y+1)] = RGB565(5,11,5);
    framebuffer[AT(x, y+2)] = RGB565(0,0,0);
    framebuffer[AT(x, y+3)] = RGB565(0,0,0);
    framebuffer[AT(x, FRAMERATE_GRAPH_MAX_Y-1)] = RGB565(0,0,0);
    framebuffer[AT(x, FRAMERATE_GRAPH_MAX_Y)] = RGB565(15,30,15);
  }
#endif
}

void RefreshStatisticsOverlayText()
{
  uint64_t now = tick();
  uint64_t elapsed = now - statsLastPrint;
  if (elapsed < STATISTICS_REFRESH_INTERVAL) return;

#ifdef FRAME_COMPLETION_TIME_STATISTICS
  if (frameCompletionTimeHistorySize > 1)
  {
    uint64_t maxInterval = 4000000 / TARGET_FRAME_RATE;
    uint64_t accumIntervals = 0;
    for(int i = 0; i < frameCompletionTimeHistorySize-1; ++i)
    {
      uint64_t interval = MIN(frameCompletionTimeHistory[i] - frameCompletionTimeHistory[i+1], maxInterval);
      accumIntervals += interval;
      statsFrameIntervalsY[i] = FRAMERATE_GRAPH_MAX_Y - (FRAMERATE_GRAPH_MAX_Y - FRAMERATE_GRAPH_MIN_Y) * interval / maxInterval;
    }
    statsTargetFrameRateY = FRAMERATE_GRAPH_MAX_Y - (FRAMERATE_GRAPH_MAX_Y - FRAMERATE_GRAPH_MIN_Y) * (1000000/TARGET_FRAME_RATE) / maxInterval;
    statsAvgFrameRateIntervalY = FRAMERATE_GRAPH_MAX_Y - (FRAMERATE_GRAPH_MAX_Y - FRAMERATE_GRAPH_MIN_Y) * (accumIntervals / (frameCompletionTimeHistorySize-1)) / maxInterval;
    statsFrameIntervalsSize = frameCompletionTimeHistorySize-1;
  }
  else
    statsFrameIntervalsSize = 0;
#endif

  UpdateStatisticsNumbers();

#ifdef USE_DMA_TRANSFERS
  sprintf(dmaChannelsText, "DMATx=%d,Rx=%d", dmaTxChannel, dmaRxChannel);
#endif
#ifdef KERNEL_MODULE_CLIENT
  spiThreadUtilizationRate = 0; // TODO
  int spiRate = 0;
  strcpy(spiUsagePercentageText, "N/A");
#else
  uint64_t spiThreadIdleFor = __atomic_load_n(&spiThreadIdleUsecs, __ATOMIC_RELAXED);
  __sync_fetch_and_sub(&spiThreadIdleUsecs, spiThreadIdleFor);
  if (__atomic_load_n(&spiThreadSleeping, __ATOMIC_RELAXED)) spiThreadIdleFor += tick() - spiThreadSleepStartTime;
  spiThreadUtilizationRate = MIN(1.0, MAX(0.0, 1.0 - spiThreadIdleFor / (double)STATISTICS_REFRESH_INTERVAL));
  int spiRate = (int)MIN(100, (spiThreadUtilizationRate*100.0));
  sprintf(spiUsagePercentageText, "%d%%", spiRate);
#endif
  spiBusDataRate = (double)8.0 * statsBytesTransferred * 1000.0 / (elapsed / 1000.0);

  if (spiRate < 90) spiUsageColor = RGB565(0,63,0);
  else if (spiRate < 100) spiUsageColor = RGB565(31,63,0);
  else spiUsageColor = RGB565(31,0, 0);

  if (spiBusDataRate > 1000000) sprintf(spiBusDataRateText, "%.2fmbps", spiBusDataRate/1000000.0);
  else if (spiBusDataRate > 1000) sprintf(spiBusDataRateText, "%.2fkbps", spiBusDataRate/1000.0);
  else sprintf(spiBusDataRateText, "%.2fbps", spiBusDataRate);

  uint64_t wastedTime = __atomic_load_n(&timeWastedPollingGPU, __ATOMIC_RELAXED);
  __atomic_fetch_sub(&timeWastedPollingGPU, wastedTime, __ATOMIC_RELAXED);
  //const double gpuPollingWastedScalingFactor = 0.369; // A crude heuristic to scale time spent in useless polling to what Linux 'top' tool shows as % usage percentages
  statsGpuPollingWasted = (int)(wastedTime /** gpuPollingWastedScalingFactor*/ * 100 / (now - statsLastPrint));

  statsBytesTransferred = 0;

  if (statsBcmCoreSpeed > 0 && statsCpuFrequency > 0) sprintf(spiSpeedText, "%d/%dMHz", statsCpuFrequency, statsBcmCoreSpeed);
  else spiSpeedText[0] = '\0';

  if (statsSpiBusSpeed > 0) sprintf(spiSpeedText2, "SPI:%.3fMHz (/%d)", statsSpiBusSpeed, spi->clk);
  else spiSpeedText2[0] = '\0';

  if (statsCpuTemperature > 0)
  {
    sprintf(cpuTemperatureText, "%.1fc", statsCpuTemperature);
    if (statsCpuTemperature >= 80) cpuTemperatureColor = RGB565(31, 0, 0);
    else if (statsCpuTemperature >= 65) cpuTemperatureColor = RGB565(31, 63, 0);
    else cpuTemperatureColor = RGB565(0, 63, 0);
  }

  if (statsGpuPollingWasted > 0)
  {
    gpuPollingWastedColor = (statsGpuPollingWasted > 5) ? RGB565(31, 0, 0) : RGB565(31, 63, 0);
    sprintf(gpuPollingWastedText, "+%d%%", statsGpuPollingWasted);
  }
  else gpuPollingWastedText[0] = '\0';

  statsLastPrint = now;

  if (frameTimeHistorySize >= 3)
  {
    int numInterlacedFramesInHistory = 0;
    int numProgressiveFramesInHistory = 0;
    for(int i = 0; i < frameTimeHistorySize; ++i)
      if (frameTimeHistory[i].interlaced)
        ++numInterlacedFramesInHistory;
      else
        ++numProgressiveFramesInHistory;

    int frames = frameTimeHistorySize;
    if (numInterlacedFramesInHistory)
      frames += numProgressiveFramesInHistory; // Progressive frames count twice as interlaced
    int fps = (0.5 + (frames - 1) * 1000000.0 / (frameTimeHistory[frameTimeHistorySize-1].time - frameTimeHistory[0].time));
#ifdef NO_INTERLACING
    sprintf(fpsText, "%d", fps);
    fpsColor = 0xFFFF;
#else
    if (numInterlacedFramesInHistory > 0)
    {
      if (numProgressiveFramesInHistory > 0) sprintf(fpsText, "%di/%d", fps, numProgressiveFramesInHistory);
      else sprintf(fpsText, "%di", fps);
      fpsColor = RGB565(31, 30, 11);
    }
    else
    {
      sprintf(fpsText, "%dp", fps);
      fpsColor = 0xFFFF;
    }
#endif
    if (frameSkipTimeHistorySize > 0) sprintf(statsFrameSkipText, "-%d", frameSkipTimeHistorySize);
    else statsFrameSkipText[0] = '\0';
  }
  else
  {
    strcpy(fpsText, "-");
    statsFrameSkipText[0] = '\0';
    fpsColor = 0xFFFF;
  }

#if (defined(DISPLAY_FLIP_ORIENTATION_IN_SOFTWARE) && DISPLAY_DRAWABLE_HEIGHT > 302) || (!defined(DISPLAY_FLIP_ORIENTATION_IN_SOFTWARE) && DISPLAY_DRAWABLE_WIDTH > 302)
#define HINTSUFFIX "MB"
#else
#define HINTSUFFIX ""
#endif

  sprintf(cpuMemoryUsedText, "CPU:%.2f" HINTSUFFIX, totalCpuMemoryAllocated/1024.0/1024.0);

#ifdef USE_DMA_TRANSFERS
  if (totalGpuMemoryUsed > 0)
    sprintf(gpuMemoryUsedText, "GPU:%.2f" HINTSUFFIX, totalGpuMemoryUsed/1024.0/1024.0);
#endif
}
#else
void RefreshStatisticsOverlayText() {}
void DrawStatisticsOverlay(uint16_t *) {}
#endif // ~STATISTICS
