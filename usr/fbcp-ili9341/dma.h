#pragma once

#ifdef USE_DMA_TRANSFERS

#define BCM2835_DMA0_OFFSET  0x7000   // DMA channels 0-14 live at 0x7E007000, offset of 0x7000 of BCM2835 peripherals base address

#define BCM2835_DMAENABLE_REGISTER_OFFSET 0xff0

typedef struct __attribute__ ((packed, aligned(4))) DMAControlBlock
{
  uint32_t ti;
  uint32_t src;
  uint32_t dst;
  uint32_t len;
  uint32_t stride;
  uint32_t next;
  uint32_t debug;
  uint32_t reserved;
} DMAControlBlock;

typedef struct __attribute__ ((packed, aligned(4))) DMAChannelRegisterFile
{
  volatile uint32_t cs;
  volatile uint32_t cbAddr;
  volatile DMAControlBlock cb;
  volatile uint8_t padding[216]; // Pad this structure to 256 bytes in size total for easy indexing into DMA channels.
} DMAChannelRegisterFile;

extern int dmaTxChannel, dmaTxIrq;
extern volatile DMAChannelRegisterFile *dmaTx; // DMA channel allocated to sending to SPI
extern int dmaRxChannel, dmaRxIrq;
extern volatile DMAChannelRegisterFile *dmaRx; // DMA channel allocated to reading from SPI

volatile DMAChannelRegisterFile *GetDMAChannel(int channelNumber);

#define BCM2835_DMA_CS_RESET                         (1<<31)
#define BCM2835_DMA_CS_ABORT                         (1<<30)
#define BCM2835_DMA_CS_DISDEBUG                      (1<<29)
#define BCM2835_DMA_CS_WAIT_FOR_OUTSTANDING_WRITES   (1<<28)
#define BCM2835_DMA_CS_PANIC_PRIORITY              (0xF<<20)
#define BCM2835_DMA_CS_PRIORITY                    (0xF<<16)
#define BCM2835_DMA_CS_ERROR                          (1<<8)
#define BCM2835_DMA_CS_WAITING_FOR_OUTSTANDING_WRITES (1<<6)
#define BCM2835_DMA_CS_DREQ_STOPS_DMA                 (1<<5)
#define BCM2835_DMA_CS_PAUSED                         (1<<4)
#define BCM2835_DMA_CS_DREQ                           (1<<3)
#define BCM2835_DMA_CS_INT                            (1<<2)
#define BCM2835_DMA_CS_END                            (1<<1)
#define BCM2835_DMA_CS_ACTIVE                         (1<<0)

#define BCM2835_DMA_CS_SET_PANIC_PRIORITY(p)          ((p) << 20)
#define BCM2835_DMA_CS_SET_PRIORITY(p)                ((p) << 16)

#define BCM2835_DMA_CS_RESET_SHIFT                          31
#define BCM2835_DMA_CS_ABORT_SHIFT                          30
#define BCM2835_DMA_CS_DISDEBUG_SHIFT                       29
#define BCM2835_DMA_CS_WAIT_FOR_OUTSTANDING_WRITES_SHIFT    28
#define BCM2835_DMA_CS_PANIC_PRIORITY_SHIFT                 20
#define BCM2835_DMA_CS_PRIORITY_SHIFT                       16
#define BCM2835_DMA_CS_ERROR_SHIFT                           8
#define BCM2835_DMA_CS_WAITING_FOR_OUTSTANDING_WRITES_SHIFT  6
#define BCM2835_DMA_CS_DREQ_STOPS_DMA_SHIFT                  5
#define BCM2835_DMA_CS_PAUSED_SHIFT                          4
#define BCM2835_DMA_CS_DREQ_SHIFT                            3
#define BCM2835_DMA_CS_INT_SHIFT                             2
#define BCM2835_DMA_CS_END_SHIFT                             1
#define BCM2835_DMA_CS_ACTIVE_SHIFT                          0

#define BCM2835_DMA_DEBUG_LITE                              (1<<28)
#define BCM2835_DMA_DEBUG_VERSION                           (7<<25)
#define BCM2835_DMA_DEBUG_DMA_STATE                         (0x1FF<<16)
#define BCM2835_DMA_DEBUG_DMA_ID                            (0xFF<<8)
#define BCM2835_DMA_DEBUG_DMA_OUTSTANDING_WRITES            (0xF<<4)
#define BCM2835_DMA_DEBUG_DMA_READ_ERROR                    (1<<2)
#define BCM2835_DMA_DEBUG_DMA_FIFO_ERROR                    (1<<1)
#define BCM2835_DMA_DEBUG_READ_LAST_NOT_SET_ERROR           (1<<0)

#define BCM2835_DMA_DEBUG_LITE_SHIFT                        28
#define BCM2835_DMA_DEBUG_VERSION_SHIFT                     25
#define BCM2835_DMA_DEBUG_DMA_STATE_SHIFT                   16
#define BCM2835_DMA_DEBUG_DMA_ID_SHIFT                      8
#define BCM2835_DMA_DEBUG_DMA_OUTSTANDING_WRITES_SHIFT      4
#define BCM2835_DMA_DEBUG_DMA_READ_ERROR_SHIFT              2
#define BCM2835_DMA_DEBUG_DMA_FIFO_ERROR_SHIFT              1
#define BCM2835_DMA_DEBUG_READ_LAST_NOT_SET_ERROR_SHIFT     0

#define BCM2835_DMA_TI_NO_WIDE_BURSTS                       (1<<26)
#define BCM2835_DMA_TI_WAITS                                (0x1F<<21)
#define BCM2835_DMA_TI_PERMAP(x)                            ((x)<<16)
#define BCM2835_DMA_TI_PERMAP_MASK                          (0x1F<<16)
#define BCM2835_DMA_TI_PERMAP_SPI_TX                        6
#define BCM2835_DMA_TI_PERMAP_SPI_RX                        7
#define BCM2835_DMA_TI_BURST_LENGTH(x)                      ((x)<<12)
#define BCM2835_DMA_TI_SRC_IGNORE                           (1<<11)
#define BCM2835_DMA_TI_SRC_DREQ                             (1<<10)
#define BCM2835_DMA_TI_SRC_WIDTH                            (1<<9)
#define BCM2835_DMA_TI_SRC_INC                              (1<<8)
#define BCM2835_DMA_TI_DEST_IGNORE                          (1<<7)
#define BCM2835_DMA_TI_DEST_DREQ                            (1<<6)
#define BCM2835_DMA_TI_DEST_WIDTH                           (1<<5)
#define BCM2835_DMA_TI_DEST_INC                             (1<<4)
#define BCM2835_DMA_TI_WAIT_RESP                            (1<<3)
#define BCM2835_DMA_TI_TDMODE                               (1<<1)
#define BCM2835_DMA_TI_INTEN                                (1<<0)

#define BCM2835_DMA_TI_NO_WIDE_BURSTS_SHIFT                 26
#define BCM2835_DMA_TI_WAITS_SHIFT                          21
#define BCM2835_DMA_TI_PERMAP_SHIFT                         16
#define BCM2835_DMA_TI_BURST_LENGTH_SHIFT                   12
#define BCM2835_DMA_TI_SRC_IGNORE_SHIFT                     11
#define BCM2835_DMA_TI_SRC_DREQ_SHIFT                       10
#define BCM2835_DMA_TI_SRC_WIDTH_SHIFT                      9
#define BCM2835_DMA_TI_SRC_INC_SHIFT                        8
#define BCM2835_DMA_TI_DEST_IGNORE_SHIFT                    7
#define BCM2835_DMA_TI_DEST_DREQ_SHIFT                      6
#define BCM2835_DMA_TI_DEST_WIDTH_SHIFT                     5
#define BCM2835_DMA_TI_DEST_INC_SHIFT                       4
#define BCM2835_DMA_TI_WAIT_RESP_SHIFT                      3
#define BCM2835_DMA_TI_TDMODE_SHIFT                         1
#define BCM2835_DMA_TI_INTEN_SHIFT                          0

// Spec sheet says there's 16 channels, but last channel is unusable:
// https://www.raspberrypi.org/forums/viewtopic.php?t=170957
// So just behave as if there are only 15 channels
#define BCM2835_NUM_DMA_CHANNELS 15

void WaitForDMAFinished(void);

// Reserves and enables a DMA channel for SPI transfers.
int InitDMA(void);
void DeinitDMA(void);

typedef struct SPITask SPITask;

void SPIDMATransfer(SPITask *task);

extern int dmaTxChannel;
extern int dmaRxChannel;
extern uint64_t totalGpuMemoryUsed;

#endif
