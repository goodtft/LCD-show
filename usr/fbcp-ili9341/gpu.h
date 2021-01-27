#pragma once

#include <inttypes.h>

void InitGPU(void);
void DeinitGPU(void);
void AddHistogramSample(uint64_t t);
bool SnapshotFramebuffer(uint16_t *destination);
bool IsNewFramebuffer(uint16_t *possiblyNewFramebuffer, uint16_t *oldFramebuffer);
uint64_t EstimateFrameRateInterval(void);
uint64_t PredictNextFrameArrivalTime(void);

extern uint16_t *videoCoreFramebuffer[2];
extern volatile int numNewGpuFrames;
extern int displayXOffset;
extern int displayYOffset;
extern int gpuFrameWidth;
extern int gpuFrameHeight;
extern int gpuFramebufferScanlineStrideBytes;
extern int gpuFramebufferSizeBytes;

extern int excessPixelsLeft;
extern int excessPixelsRight;
extern int excessPixelsTop;
extern int excessPixelsBottom;

#define FRAME_HISTORY_MAX_SIZE 240
extern int frameTimeHistorySize;

struct FrameHistory
{
  uint64_t time;
  bool interlaced;
};

extern FrameHistory frameTimeHistory[FRAME_HISTORY_MAX_SIZE];

#define HISTOGRAM_SIZE 240
extern uint64_t frameArrivalTimes[HISTOGRAM_SIZE];
extern uint64_t frameArrivalTimesTail;
extern int histogramSize;

// Returns Nth most recent entry in the frame times histogram, 0 = most recent, (histogramSize-1) = oldest
#define GET_HISTOGRAM(idx) frameArrivalTimes[(frameArrivalTimesTail - 1 - (idx) + HISTOGRAM_SIZE) % HISTOGRAM_SIZE]

// Source framebuffer captured from DispmanX is (currently) always 16-bits R5G6B5
#define FRAMEBUFFER_BYTESPERPIXEL 2
