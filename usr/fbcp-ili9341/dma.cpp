#ifndef KERNEL_MODULE
#include <stdio.h> // fprintf, stderr
#include <stdlib.h> // exit
#include <memory.h> // memset, memcpy
#include <inttypes.h> // uint32_t
#include <syslog.h> // syslog
#include <sys/mman.h> // mmap, munmap, PROT_READ, PROT_WRITE
#endif

#include "config.h"
#include "dma.h"
#include "spi.h"
#include "gpu.h"
#include "util.h"
#include "mailbox.h"

#ifdef USE_DMA_TRANSFERS

#define BCM2835_PERI_BASE               0x3F000000

SharedMemory *dmaSourceMemory = 0;
volatile DMAChannelRegisterFile *dma0 = 0;

volatile DMAChannelRegisterFile *dmaTx = 0;
volatile DMAChannelRegisterFile *dmaRx = 0;
int dmaTxChannel = -1;
int dmaTxIrq = 0;
int dmaRxChannel = -1;
int dmaRxIrq = 0;

#define PAGE_SIZE 4096

struct GpuMemory
{
  uint32_t allocationHandle;
  void *virtualAddr;
  uintptr_t busAddress;
  uint32_t sizeBytes;
};

#define NUM_DMA_CBS 1024
GpuMemory dmaCb, dmaSourceBuffer, dmaConstantData;

volatile DMAControlBlock *dmaSendTail = 0;
volatile DMAControlBlock *dmaRecvTail = 0;
volatile DMAControlBlock *firstFreeCB = 0;
volatile uint8_t *dmaSourceEnd = 0;

volatile DMAControlBlock *GrabFreeCBs(int num)
{
  volatile DMAControlBlock *firstCB = (volatile DMAControlBlock *)dmaCb.virtualAddr;
  volatile DMAControlBlock *endCB = firstCB + NUM_DMA_CBS;
  if ((uintptr_t)(firstFreeCB + num) >= (uintptr_t)dmaCb.virtualAddr + dmaCb.sizeBytes)
  {
    WaitForDMAFinished();
    firstFreeCB = firstCB;
  }

  volatile DMAControlBlock *ret = firstFreeCB;
  firstFreeCB += num;
  return ret;
}

volatile uint8_t *GrabFreeDMASourceBytes(int bytes)
{
  if ((uintptr_t)dmaSourceEnd + bytes >= (uintptr_t)dmaSourceBuffer.virtualAddr + dmaSourceBuffer.sizeBytes)
  {
    WaitForDMAFinished();
    dmaSourceEnd = (volatile uint8_t *)dmaSourceBuffer.virtualAddr;
  }

  volatile uint8_t *ret = dmaSourceEnd;
  dmaSourceEnd += bytes;
  return ret;
}

static int AllocateDMAChannel(int *dmaChannel, int *irq)
{
  // Snooping DMA, channels 3, 5 and 6 seen active.
  // TODO: Actually reserve the DMA channel to the system using bcm_dma_chan_alloc() and bcm_dma_chan_free()?...
  // Right now, use channels 1 and 4 which seem to be free.
  // Note: The send channel could be a lite channel, but receive channel cannot, since receiving uses the IGNORE flag
  // that lite DMA engines don't have.
#ifdef FREEPLAYTECH_WAVESHARE32B
  // On FreePlayTech Zero, DMA channel 4 seen to be taken by SD HOST (peripheral mapping 13).
  int freeChannels[] = { 5, 1 };
#else
  int freeChannels[] = { 7, 1 };
#endif
#if defined(DMA_TX_CHANNEL)
  freeChannels[0] = DMA_TX_CHANNEL;
#endif
#if defined(DMA_RX_CHANNEL)
  freeChannels[1] = DMA_RX_CHANNEL;
#endif
  if (freeChannels[0] == freeChannels[1]) FATAL_ERROR("DMA TX and RX channels cannot be the same channel!");

  static int nextFreeChannel = 0;
  if (nextFreeChannel >= sizeof(freeChannels) / sizeof(freeChannels[0])) FATAL_ERROR("No free DMA channels");

  *dmaChannel = freeChannels[nextFreeChannel++];
  LOG("Allocated DMA channel %d", *dmaChannel);
  *irq = 0;
  return 0;
}

void FreeDMAChannel(int channel)
{
  volatile DMAChannelRegisterFile *dma = GetDMAChannel(channel);
  dma->cb.ti = 0; // Clear the SPI TX & RX permaps for this DMA channel so that we don't think some other program is using these for SPI
}

// Message IDs for different mailbox GPU memory allocation messages
#define MEM_ALLOC_MESSAGE 0x3000c // This message is 3 u32s: numBytes, alignment and flags
#define MEM_FREE_MESSAGE 0x3000f // This message is 1 u32: handle
#define MEM_LOCK_MESSAGE 0x3000d // 1 u32: handle
#define MEM_UNLOCK_MESSAGE 0x3000e // 1 u32: handle

// Memory allocation flags
#define MEM_ALLOC_FLAG_DIRECT (1 << 2) // Allocate uncached memory that bypasses L1 and L2 cache on loads and stores
#define MEM_ALLOC_FLAG_COHERENT (1 << 3) // Non-allocating in L2 but coherent

#define BUS_TO_PHYS(x) ((x) & ~0xC0000000)

#define PHYS_TO_BUS(x) ((x) |  0xC0000000)

#define VIRT_TO_BUS(block, x) ((uintptr_t)(x) - (uintptr_t)((block).virtualAddr) + (block).busAddress)

uint64_t totalGpuMemoryUsed = 0;

// Allocates the given number of bytes in GPU side memory, and returns the virtual address and physical bus address of the allocated memory block.
// The virtual address holds an uncached view to the allocated memory, so writes and reads to that memory address bypass the L1 and L2 caches. Use
// this kind of memory to pass data blocks over to the DMA controller to process.
GpuMemory AllocateUncachedGpuMemory(uint32_t numBytes, const char *reason)
{
  GpuMemory mem;
  mem.sizeBytes = ALIGN_UP(numBytes, PAGE_SIZE);
  uint32_t allocationFlags = MEM_ALLOC_FLAG_DIRECT | MEM_ALLOC_FLAG_COHERENT;
  mem.allocationHandle = Mailbox(MEM_ALLOC_MESSAGE, /*size=*/mem.sizeBytes, /*alignment=*/PAGE_SIZE, /*flags=*/allocationFlags);
  if (!mem.allocationHandle) FATAL_ERROR("Failed to allocate GPU memory! Try increasing gpu_mem allocation in /boot/config.txt. See https://www.raspberrypi.org/documentation/configuration/config-txt/memory.md");
  mem.busAddress = Mailbox(MEM_LOCK_MESSAGE, mem.allocationHandle);
  if (!mem.busAddress) FATAL_ERROR("Failed to lock GPU memory!");
  mem.virtualAddr = mmap(0, mem.sizeBytes, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, BUS_TO_PHYS(mem.busAddress));
  if (mem.virtualAddr == MAP_FAILED) FATAL_ERROR("Failed to mmap GPU memory!");
  totalGpuMemoryUsed += mem.sizeBytes;
//  printf("Allocated %u bytes of GPU memory for %s (bus address=%p). Total GPU memory used: %llu bytes\n", mem.sizeBytes, reason, (void*)mem.busAddress, totalGpuMemoryUsed);
  return mem;
}

void FreeUncachedGpuMemory(GpuMemory mem)
{
  totalGpuMemoryUsed -= mem.sizeBytes;
  munmap(mem.virtualAddr, mem.sizeBytes);
  Mailbox(MEM_UNLOCK_MESSAGE, mem.allocationHandle);
  Mailbox(MEM_FREE_MESSAGE, mem.allocationHandle);
}

volatile DMAChannelRegisterFile *GetDMAChannel(int channelNumber)
{
  if (channelNumber < 0 || channelNumber >= BCM2835_NUM_DMA_CHANNELS)
  {
    printf("Invalid DMA channel %d specified!\n", channelNumber);
    FATAL_ERROR("Invalid DMA channel specified!");
  }
  return dma0 + channelNumber;
}

void DumpDMAPeripheralMap()
{
  for(int i = 0; i < BCM2835_NUM_DMA_CHANNELS; ++i)
  {
    volatile DMAChannelRegisterFile *channel = GetDMAChannel(i);
    printf("DMA channel %d has peripheral map %d (is lite channel: %d, currently active: %d, current control block: %p)\n", i, (channel->cb.ti & BCM2835_DMA_TI_PERMAP_MASK) >> BCM2835_DMA_TI_PERMAP_SHIFT, (channel->cb.debug & BCM2835_DMA_DEBUG_LITE) ? 1 : 0, (channel->cs & BCM2835_DMA_CS_ACTIVE) ? 1 : 0, channel->cbAddr);
  }
}

// Verifies that no other program has stomped on the DMA channel that we are using.
void CheckDMAChannelNotStolen(int channelNumber, int expectedPeripheralMap)
{
  volatile DMAChannelRegisterFile *channel = GetDMAChannel(channelNumber);
  uint32_t peripheralMap = ((channel->cb.ti & BCM2835_DMA_TI_PERMAP_MASK) >> BCM2835_DMA_TI_PERMAP_SHIFT);
  if (peripheralMap != expectedPeripheralMap && peripheralMap != 0)
  {
    DumpDMAPeripheralMap();
    printf("DMA channel collision! DMA channel %d was expected to be assigned to our peripheral %d, but something else has assigned it to peripheral %d!\n", channelNumber, expectedPeripheralMap, peripheralMap);
    FATAL_ERROR("System is likely unstable now, rebooting is advised.");
  }
  uint32_t cbAddr = channel->cbAddr;
  if (cbAddr && (cbAddr < dmaCb.busAddress || cbAddr >= dmaCb.busAddress + dmaCb.sizeBytes))
  {
    DumpDMAPeripheralMap();
    printf("DMA channel collision! Some other program has submitted a DMA task to our DMA channel %d! (DMA task at unknown control block address %p)\n", channelNumber, cbAddr);
    FATAL_ERROR("System is likely unstable now, rebooting is advised.");
  }
}

void CheckSPIDMAChannelsNotStolen()
{
  CheckDMAChannelNotStolen(dmaTxChannel, BCM2835_DMA_TI_PERMAP_SPI_TX);
  CheckDMAChannelNotStolen(dmaRxChannel, BCM2835_DMA_TI_PERMAP_SPI_RX);
}

void ResetDMAChannels()
{
  dmaTx->cs = BCM2835_DMA_CS_RESET;
  dmaTx->cb.debug = BCM2835_DMA_DEBUG_DMA_READ_ERROR | BCM2835_DMA_DEBUG_DMA_FIFO_ERROR | BCM2835_DMA_DEBUG_READ_LAST_NOT_SET_ERROR;
  dmaRx->cs = BCM2835_DMA_CS_RESET;
  dmaRx->cb.debug = BCM2835_DMA_DEBUG_DMA_READ_ERROR | BCM2835_DMA_DEBUG_DMA_FIFO_ERROR | BCM2835_DMA_DEBUG_READ_LAST_NOT_SET_ERROR;
}

int InitDMA()
{
#if defined(KERNEL_MODULE)
  dma0 = (volatile DMAChannelRegisterFile*)ioremap(BCM2835_PERI_BASE+BCM2835_DMA0_OFFSET, BCM2835_NUM_DMA_CHANNELS*0x100);
#else
  dma0 = (volatile DMAChannelRegisterFile*)((uintptr_t)bcm2835 + BCM2835_DMA0_OFFSET);
#endif

#ifdef KERNEL_MODULE_CLIENT
  dmaTxChannel = spiTaskMemory->dmaTxChannel;
  dmaRxChannel = spiTaskMemory->dmaRxChannel;
#else
  int ret = AllocateDMAChannel(&dmaTxChannel, &dmaTxIrq);
  if (ret != 0) FATAL_ERROR("Unable to allocate TX DMA channel!");
  ret = AllocateDMAChannel(&dmaRxChannel, &dmaRxIrq);
  if (ret != 0) FATAL_ERROR("Unable to allocate RX DMA channel!");

  printf("Enabling DMA channels Tx:%d and Rx:%d\n", dmaTxChannel, dmaRxChannel);
  volatile uint32_t *dmaEnableRegister = (volatile uint32_t *)((uintptr_t)dma0 + BCM2835_DMAENABLE_REGISTER_OFFSET);

  // Enable the allocated DMA channels
  *dmaEnableRegister |= (1 << dmaTxChannel);
  *dmaEnableRegister |= (1 << dmaRxChannel);
#endif

#if !defined(KERNEL_MODULE)
  dmaCb = AllocateUncachedGpuMemory(sizeof(DMAControlBlock) * NUM_DMA_CBS, "DMA control blocks");
  memset(dmaCb.virtualAddr, 0, dmaCb.sizeBytes); // Some fields of the CBs (debug, reserved) are initialized to zero and assumed to stay so throughout app lifetime.
  firstFreeCB = (volatile DMAControlBlock *)dmaCb.virtualAddr;

  dmaSourceBuffer = AllocateUncachedGpuMemory(SHARED_MEMORY_SIZE*2, "DMA source data");
  dmaSourceEnd = (volatile uint8_t *)dmaSourceBuffer.virtualAddr;

  dmaConstantData = AllocateUncachedGpuMemory(2*sizeof(uint32_t), "DMA constant data");
  uint32_t *constantData = (uint32_t *)dmaConstantData.virtualAddr;
  constantData[0] = BCM2835_SPI0_CS_DMAEN; // constantData[0] is for disableTransferActive task
  constantData[1] = BCM2835_DMA_CS_ACTIVE | BCM2835_DMA_CS_END; // constantData[1] is for startDMATxChannel task
#endif

  LOG("DMA hardware register file is at ptr: %p, using DMA TX channel: %d and DMA RX channel: %d", dma0, dmaTxChannel, dmaRxChannel);
  if (!dma0) FATAL_ERROR("Failed to map DMA!");

  dmaTx = GetDMAChannel(dmaTxChannel);
  dmaRx = GetDMAChannel(dmaRxChannel);
  LOG("DMA hardware TX channel register file is at ptr: %p, DMA RX channel register file is at ptr: %p", dmaTx, dmaRx);
  int dmaTxPeripheralMap = (dmaTx->cb.ti & BCM2835_DMA_TI_PERMAP_MASK) >> BCM2835_DMA_TI_PERMAP_SHIFT;
  if (dmaTxPeripheralMap != 0 && dmaTxPeripheralMap != BCM2835_DMA_TI_PERMAP_SPI_TX)
  {
    DumpDMAPeripheralMap();
    LOG("DMA TX channel %d was assigned another peripheral map %d!", dmaTxChannel, dmaTxPeripheralMap);
    FATAL_ERROR("DMA TX channel was assigned another peripheral map!");
  }
  if (dmaTx->cbAddr != 0 && (dmaTx->cs & BCM2835_DMA_CS_ACTIVE))
    FATAL_ERROR("DMA TX channel was in use!");

  int dmaRxPeripheralMap = (dmaRx->cb.ti & BCM2835_DMA_TI_PERMAP_MASK) >> BCM2835_DMA_TI_PERMAP_SHIFT;
  if (dmaRxPeripheralMap != 0 && dmaRxPeripheralMap != BCM2835_DMA_TI_PERMAP_SPI_RX)
  {
    LOG("DMA RX channel %d was assigned another peripheral map %d!", dmaRxChannel, dmaRxPeripheralMap);
    DumpDMAPeripheralMap();
    FATAL_ERROR("DMA RX channel was assigned another peripheral map!");
  }
  if (dmaRx->cbAddr != 0 && (dmaRx->cs & BCM2835_DMA_CS_ACTIVE))
    FATAL_ERROR("DMA RX channel was in use!");

  if ((dmaRx->cb.debug & BCM2835_DMA_DEBUG_LITE) != 0)
    FATAL_ERROR("DMA RX channel cannot be a lite channel, because to get best performance we want to use BCM2835_DMA_TI_DEST_IGNORE DMA operation mode that lite DMA channels do not have. (Try using DMA RX channel value < 7)");

  LOG("Resetting DMA channels for use");
  ResetDMAChannels();

  // TODO: Set up IRQ
  LOG("DMA all set up");
  return 0;
}

// Debugging functions to introspect SPI and DMA hardware registers:

void DumpCS(uint32_t reg)
{
  PRINT_FLAG(BCM2835_DMA_CS_RESET);
  PRINT_FLAG(BCM2835_DMA_CS_ABORT);
  PRINT_FLAG(BCM2835_DMA_CS_DISDEBUG);
  PRINT_FLAG(BCM2835_DMA_CS_WAIT_FOR_OUTSTANDING_WRITES);
  PRINT_FLAG(BCM2835_DMA_CS_PANIC_PRIORITY);
  PRINT_FLAG(BCM2835_DMA_CS_PRIORITY);
  PRINT_FLAG(BCM2835_DMA_CS_ERROR);
  PRINT_FLAG(BCM2835_DMA_CS_WAITING_FOR_OUTSTANDING_WRITES);
  PRINT_FLAG(BCM2835_DMA_CS_DREQ_STOPS_DMA);
  PRINT_FLAG(BCM2835_DMA_CS_PAUSED);
  PRINT_FLAG(BCM2835_DMA_CS_DREQ);
  PRINT_FLAG(BCM2835_DMA_CS_INT);
  PRINT_FLAG(BCM2835_DMA_CS_END);
  PRINT_FLAG(BCM2835_DMA_CS_ACTIVE);
}

void DumpDebug(uint32_t reg)
{
  PRINT_FLAG(BCM2835_DMA_DEBUG_LITE);
  PRINT_FLAG(BCM2835_DMA_DEBUG_VERSION);
  PRINT_FLAG(BCM2835_DMA_DEBUG_DMA_STATE);
  PRINT_FLAG(BCM2835_DMA_DEBUG_DMA_ID);
  PRINT_FLAG(BCM2835_DMA_DEBUG_DMA_OUTSTANDING_WRITES);
  PRINT_FLAG(BCM2835_DMA_DEBUG_DMA_READ_ERROR);
  PRINT_FLAG(BCM2835_DMA_DEBUG_DMA_FIFO_ERROR);
  PRINT_FLAG(BCM2835_DMA_DEBUG_READ_LAST_NOT_SET_ERROR);
}

void DumpTI(uint32_t reg)
{
  PRINT_FLAG(BCM2835_DMA_TI_NO_WIDE_BURSTS);
  PRINT_FLAG(BCM2835_DMA_TI_WAITS);
#define BCM2835_DMA_TI_PERMAP_MASK_SHIFT                    16
  PRINT_FLAG(BCM2835_DMA_TI_PERMAP_MASK);
//  PRINT_FLAG(BCM2835_DMA_TI_BURST_LENGTH);
  PRINT_FLAG(BCM2835_DMA_TI_SRC_IGNORE);
  PRINT_FLAG(BCM2835_DMA_TI_SRC_DREQ);
  PRINT_FLAG(BCM2835_DMA_TI_SRC_WIDTH);
  PRINT_FLAG(BCM2835_DMA_TI_SRC_INC);
  PRINT_FLAG(BCM2835_DMA_TI_DEST_IGNORE);
  PRINT_FLAG(BCM2835_DMA_TI_DEST_DREQ);
  PRINT_FLAG(BCM2835_DMA_TI_DEST_WIDTH);
  PRINT_FLAG(BCM2835_DMA_TI_DEST_INC);
  PRINT_FLAG(BCM2835_DMA_TI_WAIT_RESP);
  PRINT_FLAG(BCM2835_DMA_TI_TDMODE);
  PRINT_FLAG(BCM2835_DMA_TI_INTEN);
}

#define DMA_DMA0_CB_PHYS_ADDRESS 0x7E007000

#define DMA_SPI_CS_PHYS_ADDRESS 0x7E204000
#define DMA_SPI_FIFO_PHYS_ADDRESS 0x7E204004
#define DMA_SPI_DLEN_PHYS_ADDRESS 0x7E20400C
#define DMA_GPIO_SET_PHYS_ADDRESS 0x7E20001C
#define DMA_GPIO_CLEAR_PHYS_ADDRESS 0x7E200028

void DumpDMAState()
{
  printf("---SPI:---\n");
  DumpSPICS(spi->cs);
  printf("---DMATX CS:---\n");
  DumpCS(dmaTx->cs);
  printf("---DMATX TI:---\n");
  DumpTI(dmaTx->cb.ti);
  printf("---DMATX DEBUG:---\n");
  DumpDebug(dmaTx->cb.debug);
  printf("****** DMATX cbAddr: %p\n", dmaTx->cbAddr);

  printf("---DMARX CS:---\n");
  DumpCS(dmaRx->cs);
  printf("---DMARX TI:---\n");
  DumpTI(dmaRx->cb.ti);
  printf("---DMARX DEBUG:---\n");
  DumpDebug(dmaRx->cb.debug);
  printf("****** DMARX cbAddr: %p\n", dmaRx->cbAddr);
}

extern volatile bool programRunning;

void WaitForDMAFinished()
{
  int spins = 0;
  uint64_t t0 = tick();
  while((dmaTx->cs & BCM2835_DMA_CS_ACTIVE) && programRunning)
  {
    usleep(100);
    if (tick() - t0 > 2000000)
    {
      printf("TX stalled\n");
      DumpDMAState();
      exit(1);
    }
  }
  spins = 0;
  t0 = tick();
  while((dmaRx->cs & BCM2835_DMA_CS_ACTIVE) && programRunning)
  {
    usleep(100);
    if (tick() - t0 > 2000000)
    {
      printf("RX stalled\n");
      DumpDMAState();
      exit(1);
    }
  }
  dmaSendTail = 0;
  dmaRecvTail = 0;
}

#ifdef ALL_TASKS_SHOULD_DMA

// This function does a memcpy from one source buffer to two destination buffers simultaneously.
// It saves a lot of time on ARMv6 by avoiding to have to do two separate memory copies, because the ARMv6 L1 cache is so tiny (4K) that it cannot fit a whole framebuffer
// in memory at a time. Streaming through it only once instead of twice helps memory bandwidth immensely, this is profiled to be ~4x faster than a pair of memcpys or a simple CPU loop.
// In addition, this does a little endian->big endian conversion when copying data out to dstDma.
static void memcpy_to_dma_and_prev_framebuffer(uint16_t *dstDma, uint16_t **dstPrevFramebuffer, uint16_t **srcFramebuffer, int numBytes, int *taskStartX, int width, int stride)
{
  int strideEnd = stride - width*2;
  int xLeft = width-*taskStartX;

  uint16_t *Src = *srcFramebuffer;
  uint16_t *Dst1 = *dstPrevFramebuffer;

  // TODO: Do the loops in aligned order with unaligned head and tail separate, and ensure that dstDma, dstPrevFramebuffer and srcFramebuffer are in same alignment phase.
  asm volatile(
  "start_%=:\n"
    "ldrd r0, r1, [%[srcFramebuffer]], #8\n"
    "pld [%[srcFramebuffer], #248]\n"
    "strd r0, r1, [%[dstPrevFramebuffer]], #8\n"
    "rev16 r0, r0\n"
    "rev16 r1, r1\n"
    "strd r0, r1, [%[dstDma]], #8\n"

    "ldrd r0, r1, [%[srcFramebuffer]], #8\n"
    "strd r0, r1, [%[dstPrevFramebuffer]], #8\n"
    "rev16 r0, r0\n"
    "rev16 r1, r1\n"
    "strd r0, r1, [%[dstDma]], #8\n"

    "ldrd r0, r1, [%[srcFramebuffer]], #8\n"
    "strd r0, r1, [%[dstPrevFramebuffer]], #8\n"
    "rev16 r0, r0\n"
    "rev16 r1, r1\n"
    "strd r0, r1, [%[dstDma]], #8\n"

    "ldrd r0, r1, [%[srcFramebuffer]], #8\n"
    "strd r0, r1, [%[dstPrevFramebuffer]], #8\n"
    "rev16 r0, r0\n"
    "rev16 r1, r1\n"
    "strd r0, r1, [%[dstDma]], #8\n"

    "subs %[xLeft], %[xLeft], #16\n"
    "addls %[xLeft], %[xLeft], %[width]\n"
    "addls %[dstPrevFramebuffer], %[dstPrevFramebuffer], %[strideEnd]\n"
    "addls %[srcFramebuffer], %[srcFramebuffer], %[strideEnd]\n"

    "subs %[numBytes], %[numBytes], #32\n"
    "bhi start_%=\n"

    : [dstDma]"+r"(dstDma), [dstPrevFramebuffer]"+r"(Dst1), [srcFramebuffer]"+r"(Src), [xLeft]"+r"(xLeft), [numBytes]"+r"(numBytes)
    : [strideEnd]"r"(strideEnd), [width]"r"(width)
    : "r0", "r1", "memory", "cc"
  );
  *taskStartX = width - xLeft;
  *srcFramebuffer = Src;
  *dstPrevFramebuffer = Dst1;
}

static void memcpy_to_dma_and_prev_framebuffer_in_c(uint16_t *dstDma, uint16_t **dstPrevFramebuffer, uint16_t **srcFramebuffer, int numBytes, int *taskStartX, int width, int stride)
{
  static bool performanceWarningPrinted = false;
  if (!performanceWarningPrinted)
  {
    printf("Performance warning: using slow memcpy_to_dma_and_prev_framebuffer_in_c() function. Check conditions in display.h that enable OFFLOAD_PIXEL_COPY_TO_DMA_CPP and configure to use that instead.\n");
    performanceWarningPrinted = true;
  }
  int numPixels = numBytes>>1;
  int endStridePixels = (stride>>1) - width;
  uint16_t *prevData = *dstPrevFramebuffer;
  uint16_t *data = *srcFramebuffer;
  for(int i = 0; i < numPixels; ++i)
  {
    *prevData++ = *data;
    dstDma[i] = __builtin_bswap16(*data++);
    if (++*taskStartX >= width)
    {
      *taskStartX = 0;
      data += endStridePixels;
      prevData += endStridePixels;
    }
  }
  *srcFramebuffer = data;
  *dstPrevFramebuffer = prevData;
}

#if defined(ALL_TASKS_SHOULD_DMA) && defined(SPI_3WIRE_PROTOCOL)
// Bug: there is something about the chained DMA transfer mechanism that makes write window coordinate set commands not go through properly
// on 3-wire displays, but do not yet know what. (Remove this #error statement to debug)
#error ALL_TASKS_SHOULD_DMA and SPI_3WIRE_PROTOCOL are currently not mutually compatible!
#endif

#if defined(OFFLOAD_PIXEL_COPY_TO_DMA_CPP) && defined(SPI_3WIRE_PROTOCOL)
// We would have to convert 8-bit tasks to 9-bit tasks immediately after offloaded memcpy has been done below to implement this.
#error OFFLOAD_PIXEL_COPY_TO_DMA_CPP and SPI_3WIRE_PROTOCOL are not mutually compatible!
#endif

void SPIDMATransfer(SPITask *task)
{
// There is a limit to how many bytes can be sent in one DMA-based SPI task, so if the task
// is larger than this, we'll split the send into multiple individual DMA SPI transfers
// and chain them together. This should be a multiple of 32 bytes to keep tasks cache aligned on ARMv6.
#define MAX_DMA_SPI_TASK_SIZE 65504

  const int numDMASendTasks = (task->PayloadSize() + MAX_DMA_SPI_TASK_SIZE - 1) / MAX_DMA_SPI_TASK_SIZE;

  volatile uint32_t *dmaData = (volatile uint32_t *)GrabFreeDMASourceBytes(4*(numDMASendTasks-1)+4*numDMASendTasks+task->PayloadSize());
  volatile uint32_t *setDMATxAddressData = dmaData;
  volatile uint32_t *txData = dmaData+numDMASendTasks-1;

  volatile DMAControlBlock *cb = GrabFreeCBs(numDMASendTasks*5-3);

  volatile DMAControlBlock *rxTail = 0;
  volatile DMAControlBlock *tx0 = &cb[0];
  volatile DMAControlBlock *rx0 = &cb[1];

#ifdef OFFLOAD_PIXEL_COPY_TO_DMA_CPP
  uint8_t *data = task->fb;
  uint8_t *prevData = task->prevFb;
  const bool taskAndFramebufferSizesCompatibleWithTightMemcpy = (task->PayloadSize() % 32 == 0) && (task->width % 16 == 0);
#else
  uint8_t *data = task->PayloadStart();
#endif

  int bytesLeft = task->PayloadSize();
  int taskStartX = 0;

  while(bytesLeft > 0)
  {
    int sendSize = MIN(bytesLeft, MAX_DMA_SPI_TASK_SIZE);
    bytesLeft -= sendSize;

    volatile DMAControlBlock *tx = cb++;
    txData[0] = BCM2835_SPI0_CS_TA | DISPLAY_SPI_DRIVE_SETTINGS | (sendSize << 16); // The first four bytes written to the SPI data register control the DLEN and CS,CPOL,CPHA settings.
    // This is really sad: we must do a memcpy to prepare for DMA controller to be able to do a memcpy. The reason for this is that the DMA source memory area must be in cache bypassing
    // region of memory, which the SPI source ring buffer is not. It could be allocated to be so however, but bypassing the caches on the SPI ring buffer would cause a massive -51.5%
    // profiled overall performance drop (tested on Pi3B+ and Tontec 3.5" 480x320 display on gpu test pattern, see branch non_intermediate_memcpy_for_dma). Therefore just keep doing
    // this memcpy() to prepare for DMA to do its memcpy(), as it is faster overall. (If there was a way to map same physical memory to virtual address space twice, once cached, and
    // another time uncached, and have writes bypass the cache and only write combine, but have reads follow the cache, then it might work without a perf hit, but not at all sure if
    // that would be technically possible)
    uint16_t *txPtr = (uint16_t*)(txData+1);

    // If task->prevFb is present, the DMA backend is responsible for streaming pixel data from current framebuffer to old framebuffer, and the DMA task buffer.
    // If not present, then that preparation has been already done by the caller.
#ifdef OFFLOAD_PIXEL_COPY_TO_DMA_CPP
    if (prevData)
    {
      // For 2D pixel data, do a "everything in one pass"
      if (taskAndFramebufferSizesCompatibleWithTightMemcpy)
        memcpy_to_dma_and_prev_framebuffer((uint16_t*)txPtr, (uint16_t**)&prevData, (uint16_t**)&data, sendSize, &taskStartX, task->width, gpuFramebufferScanlineStrideBytes);
      else
        memcpy_to_dma_and_prev_framebuffer_in_c((uint16_t*)txPtr, (uint16_t**)&prevData, (uint16_t**)&data, sendSize, &taskStartX, task->width, gpuFramebufferScanlineStrideBytes);
    }
    else
#endif
    {
      memcpy(txPtr, data, sendSize);
      data += sendSize;
    }

    tx->ti = BCM2835_DMA_TI_PERMAP(BCM2835_DMA_TI_PERMAP_SPI_TX) | BCM2835_DMA_TI_DEST_DREQ | BCM2835_DMA_TI_SRC_INC | BCM2835_DMA_TI_WAIT_RESP;
    tx->src = VIRT_TO_BUS(dmaSourceBuffer, txData);
    tx->dst = DMA_SPI_FIFO_PHYS_ADDRESS; // Write out to the SPI peripheral
    tx->len = 4+sendSize;
    tx->next = 0;
    txData += 1+sendSize/4;

    volatile DMAControlBlock *rx = cb++;
    rx->ti = BCM2835_DMA_TI_PERMAP(BCM2835_DMA_TI_PERMAP_SPI_RX) | BCM2835_DMA_TI_SRC_DREQ | BCM2835_DMA_TI_DEST_IGNORE;
    rx->src = DMA_SPI_FIFO_PHYS_ADDRESS;
    rx->dst = 0;
    rx->len = sendSize;
    rx->next = 0;

    if (rxTail)
    {
      volatile DMAControlBlock *setDMATxAddress = cb++;
      volatile DMAControlBlock *disableTransferActive = cb++;
      volatile DMAControlBlock *startDMATxChannel = cb++;

      rxTail->next = VIRT_TO_BUS(dmaCb, setDMATxAddress);

      setDMATxAddressData[0] = VIRT_TO_BUS(dmaCb, tx);
      setDMATxAddress->ti = BCM2835_DMA_TI_SRC_INC | BCM2835_DMA_TI_DEST_INC | BCM2835_DMA_TI_WAIT_RESP;
      setDMATxAddress->src = VIRT_TO_BUS(dmaSourceBuffer, setDMATxAddressData);
      setDMATxAddress->dst = DMA_DMA0_CB_PHYS_ADDRESS + dmaTxChannel*0x100 + 4;
      setDMATxAddress->len = 4;
      setDMATxAddress->next = VIRT_TO_BUS(dmaCb, disableTransferActive);
      ++setDMATxAddressData;

      disableTransferActive->ti = BCM2835_DMA_TI_SRC_INC | BCM2835_DMA_TI_DEST_INC | BCM2835_DMA_TI_WAIT_RESP;
      disableTransferActive->src = dmaConstantData.busAddress;
      disableTransferActive->dst = DMA_SPI_CS_PHYS_ADDRESS;
      disableTransferActive->len = 4;
      disableTransferActive->next = VIRT_TO_BUS(dmaCb, startDMATxChannel);

      startDMATxChannel->ti = BCM2835_DMA_TI_SRC_INC | BCM2835_DMA_TI_DEST_INC | BCM2835_DMA_TI_WAIT_RESP;
      startDMATxChannel->src = dmaConstantData.busAddress+4;
      startDMATxChannel->dst = DMA_DMA0_CB_PHYS_ADDRESS + dmaTxChannel*0x100;
      startDMATxChannel->len = 4;
      startDMATxChannel->next = VIRT_TO_BUS(dmaCb, rx);

    }
    rxTail = rx;
  }

  static uint64_t taskStartTime = 0;
  static int pendingTaskBytes = 1;
  double pendingTaskUSecs = pendingTaskBytes * spiUsecsPerByte;
  pendingTaskUSecs -= tick() - taskStartTime;
  if (pendingTaskUSecs > 70)
    usleep(pendingTaskUSecs-70);

  uint64_t dmaTaskStart = tick();

  CheckSPIDMAChannelsNotStolen();
  while((dmaTx->cs & BCM2835_DMA_CS_ACTIVE) && programRunning)
  {
    usleep(250);
    CheckSPIDMAChannelsNotStolen();
    if (tick() - dmaTaskStart > 5000000)
    {
      DumpDMAState();
      FATAL_ERROR("DMA TX channel has stalled!");
    }
  }
  while((dmaRx->cs & BCM2835_DMA_CS_ACTIVE) && programRunning)
  {
    usleep(250);
    CheckSPIDMAChannelsNotStolen();
    if (tick() - dmaTaskStart > 5000000)
    {
      DumpDMAState();
      FATAL_ERROR("DMA RX channel has stalled!");
    }
  }
  if (!programRunning) return;

  pendingTaskBytes = task->PayloadSize();

  // First send the SPI command byte in Polled SPI mode
  spi->cs = BCM2835_SPI0_CS_TA | BCM2835_SPI0_CS_CLEAR | DISPLAY_SPI_DRIVE_SETTINGS;

#ifndef SPI_3WIRE_PROTOCOL
  CLEAR_GPIO(GPIO_TFT_DATA_CONTROL);
#ifdef DISPLAY_SPI_BUS_IS_16BITS_WIDE
  spi->fifo = 0;
  spi->fifo = task->cmd;
  while(!(spi->cs & (BCM2835_SPI0_CS_DONE))) /*nop*/;
  // spi->fifo; // Currently no need to flush these, the clear below clears the rx queue.
  // spi->fifo;
#else
  spi->fifo = task->cmd;
  while(!(spi->cs & (BCM2835_SPI0_CS_RXD|BCM2835_SPI0_CS_DONE))) /*nop*/;
  // spi->fifo; // Currently no need to flush this, the clear below clears the rx queue.
#endif

  SET_GPIO(GPIO_TFT_DATA_CONTROL);
#endif

  spi->cs = BCM2835_SPI0_CS_DMAEN | BCM2835_SPI0_CS_CLEAR | DISPLAY_SPI_DRIVE_SETTINGS;

  dmaTx->cbAddr = VIRT_TO_BUS(dmaCb, tx0);
  dmaRx->cbAddr = VIRT_TO_BUS(dmaCb, rx0);
  __sync_synchronize();
  dmaTx->cs = BCM2835_DMA_CS_ACTIVE | BCM2835_DMA_CS_END;
  dmaRx->cs = BCM2835_DMA_CS_ACTIVE | BCM2835_DMA_CS_END;
  taskStartTime = tick();
}

#else

void SPIDMATransfer(SPITask *task)
{
  // Transition the SPI peripheral to enable the use of DMA
  spi->cs = BCM2835_SPI0_CS_DMAEN | BCM2835_SPI0_CS_CLEAR | DISPLAY_SPI_DRIVE_SETTINGS;
  uint32_t *headerAddr = task->DmaSpiHeaderAddress();
  *headerAddr = BCM2835_SPI0_CS_TA | DISPLAY_SPI_DRIVE_SETTINGS | (task->PayloadSize() << 16); // The first four bytes written to the SPI data register control the DLEN and CS,CPOL,CPHA settings.

  // TODO: Ideally we would be able to directly perform the DMA from the SPI ring buffer from 'task' pointer. However
  // that pointer is shared to userland, and it is proving troublesome to make it both userland-writable as well as cache-bypassing DMA coherent.
  // Therefore these two memory areas are separate for now, and we memcpy() from SPI ring buffer to an intermediate 'dmaSourceMemory' memory area to perform
  // the DMA transfer. Is there a way to avoid this intermediate buffer? That would improve performance a bit.
  memcpy(dmaSourceBuffer.virtualAddr, headerAddr, task->PayloadSize() + 4);

  volatile DMAControlBlock *cb = (volatile DMAControlBlock *)dmaCb.virtualAddr;
  volatile DMAControlBlock *txcb = &cb[0];
  txcb->ti = BCM2835_DMA_TI_PERMAP(BCM2835_DMA_TI_PERMAP_SPI_TX) | BCM2835_DMA_TI_DEST_DREQ | BCM2835_DMA_TI_SRC_INC | BCM2835_DMA_TI_WAIT_RESP;
  txcb->src = dmaSourceBuffer.busAddress;
  txcb->dst = DMA_SPI_FIFO_PHYS_ADDRESS; // Write out to the SPI peripheral 
  txcb->len = task->PayloadSize() + 4;
  txcb->stride = 0;
  txcb->next = 0;
  txcb->debug = 0;
  txcb->reserved = 0;
  dmaTx->cbAddr = dmaCb.busAddress;

  volatile DMAControlBlock *rxcb = &cb[1];
  rxcb->ti = BCM2835_DMA_TI_PERMAP(BCM2835_DMA_TI_PERMAP_SPI_RX) | BCM2835_DMA_TI_SRC_DREQ | BCM2835_DMA_TI_DEST_IGNORE;
  rxcb->src = DMA_SPI_FIFO_PHYS_ADDRESS;
  rxcb->dst = 0;
  rxcb->len = task->PayloadSize();
  rxcb->stride = 0;
  rxcb->next = 0;
  rxcb->debug = 0;
  rxcb->reserved = 0;
  dmaRx->cbAddr = dmaCb.busAddress + sizeof(DMAControlBlock);

  __sync_synchronize();
  dmaTx->cs = BCM2835_DMA_CS_ACTIVE;
  dmaRx->cs = BCM2835_DMA_CS_ACTIVE;
  __sync_synchronize();

  double pendingTaskUSecs = task->PayloadSize() * spiUsecsPerByte;
  if (pendingTaskUSecs > 70)
    usleep(pendingTaskUSecs-70);

  uint64_t dmaTaskStart = tick();

  CheckSPIDMAChannelsNotStolen();
  while((dmaTx->cs & BCM2835_DMA_CS_ACTIVE))
  {
    CheckSPIDMAChannelsNotStolen();
    if (tick() - dmaTaskStart > 5000000)
      FATAL_ERROR("DMA TX channel has stalled!");
  }
  while((dmaRx->cs & BCM2835_DMA_CS_ACTIVE))
  {
    CheckSPIDMAChannelsNotStolen();
    if (tick() - dmaTaskStart > 5000000)
      FATAL_ERROR("DMA RX channel has stalled!");
  }

  __sync_synchronize();
  spi->cs = BCM2835_SPI0_CS_TA | BCM2835_SPI0_CS_CLEAR | DISPLAY_SPI_DRIVE_SETTINGS;
  __sync_synchronize();
}

#endif

void DeinitDMA(void)
{
  WaitForDMAFinished();
  ResetDMAChannels();
  FreeUncachedGpuMemory(dmaSourceBuffer);
  FreeUncachedGpuMemory(dmaCb);
  FreeUncachedGpuMemory(dmaConstantData);
  if (dmaTxChannel != -1)
  {
    FreeDMAChannel(dmaTxChannel);
    dmaTxChannel = -1;
  }
  if (dmaRxChannel != -1)
  {
    FreeDMAChannel(dmaRxChannel);
    dmaRxChannel = -1;
  }
}


#endif // ~USE_DMA_TRANSFERS
