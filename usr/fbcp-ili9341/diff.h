#pragma once

#include <inttypes.h>

// Spans track dirty rectangular areas on screen
struct Span
{
  uint16_t x, endX, y, endY, lastScanEndX;
  uint32_t size; // Specifies a box of width [x, endX[ * [y, endY[, where scanline endY-1 can be partial, and ends in lastScanEndX.
  Span *next; // Maintain a linked skip list inside the array for fast seek to next active element when pruning
};

extern Span *spans;

// Looking at SPI communication in a logic analyzer, it is observed that waiting for the finish of an SPI command FIFO causes pretty exactly one byte of delay to the command stream.
// Therefore the time/bandwidth cost of ending the current span and starting a new span is as follows:
// 1 byte to wait for the current SPI FIFO batch to finish,
// +1 byte to send the cursor X coordinate change command,
// +1 byte to wait for that FIFO to flush,
// +2 bytes to send the new X coordinate,
// +1 byte to wait for the FIFO to flush again,
// +1 byte to send the data_write command,
// +1 byte to wait for that FIFO to flush,
// after which the communication is ready to start pushing pixels. This totals to 8 bytes, or 4 pixels, meaning that if there are 4 unchanged pixels or less between two adjacent dirty
// spans, it is all the same to just update through those pixels as well to not have to wait to flush the FIFO.
#if defined(ALL_TASKS_SHOULD_DMA)
#define SPAN_MERGE_THRESHOLD 320
#elif defined(DISPLAY_SPI_BUS_IS_16BITS_WIDE)
#define SPAN_MERGE_THRESHOLD 10
#elif defined(HX8357D)
#define SPAN_MERGE_THRESHOLD 6
#else
#define SPAN_MERGE_THRESHOLD 4
#endif

void DiffFramebuffersToSingleChangedRectangle(uint16_t *framebuffer, uint16_t *prevFramebuffer, Span *&head);

void DiffFramebuffersToScanlineSpansExact(uint16_t *framebuffer, uint16_t *prevFramebuffer, bool interlacedDiff, int interlacedFieldParity, Span *&head);

void DiffFramebuffersToScanlineSpansFastAndCoarse4Wide(uint16_t *framebuffer, uint16_t *prevFramebuffer, bool interlacedDiff, int interlacedFieldParity, Span *&head);

void NoDiffChangedRectangle(Span *&head);

void MergeScanlineSpanList(Span *listHead);
