#include <linux/buffer_head.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/fb.h>
#include <linux/fs.h>
#include <linux/futex.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/math64.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_data/dma-bcm2708.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/spi/spidev.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <asm/io.h>
#include <asm/segment.h>
#include <asm/uaccess.h>

#include "../config.h"
#include "../display.h"
#include "../spi.h"
#include "../util.h"
#include "../dma.h"

static inline uint64_t tick(void)
{
    struct timespec start = current_kernel_time();
    return start.tv_sec * 1000000 + start.tv_nsec / 1000;
}

// TODO: Super-dirty temp, factor this into kbuild Makefile.
#include "../spi.cpp"
#include "../dma.cpp"

volatile SPITask *currentTask = 0;
volatile uint8_t *taskNextByte = 0;
volatile uint8_t *taskEndByte = 0;

#define SPI_BUS_PROC_ENTRY_FILENAME "bcm2835_spi_display_bus"

typedef struct mmap_info
{
  char *data;
} mmap_info;

static void p_vm_open(struct vm_area_struct *vma)
{
}

static void p_vm_close(struct vm_area_struct *vma)
{
}

static int p_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
  mmap_info *info = (mmap_info *)vma->vm_private_data;
  if (info->data)
  {
    struct page *page = virt_to_page(info->data + vmf->pgoff*PAGE_SIZE);
    get_page(page);
    vmf->page = page;
  }
  return 0;
}

static struct vm_operations_struct vm_ops =
{
  .open = p_vm_open,
  .close = p_vm_close,
  .fault = p_vm_fault,
};

static int p_mmap(struct file *filp, struct vm_area_struct *vma)
{
  vma->vm_ops = &vm_ops;
  vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;
  vma->vm_private_data = filp->private_data;
  p_vm_open(vma);
  return 0;
}

static int p_open(struct inode *inode, struct file *filp)
{
  mmap_info *info = kmalloc(sizeof(mmap_info), GFP_KERNEL);
  info->data = (void*)spiTaskMemory;
  filp->private_data = info;
  return 0;
}

static int p_release(struct inode *inode, struct file *filp)
{
  mmap_info *info;
  info = filp->private_data;
  kfree(info);
  filp->private_data = NULL;
  return 0;
}

static const struct file_operations fops =
{
  .mmap = p_mmap,
  .open = p_open,
  .release = p_release,
};

#ifdef KERNEL_DRIVE_WITH_IRQ
static irqreturn_t irq_handler(int irq, void* dev_id)
{
#ifndef KERNEL_MODULE_CLIENT_DRIVES
  uint32_t cs = spi->cs;
  if (!taskNextByte)
  {
    if (currentTask) DoneTask((SPITask*)currentTask);
    currentTask = GetTask();
    if (!currentTask)
    {
      spi->cs = (cs & ~BCM2835_SPI0_CS_TA) | BCM2835_SPI0_CS_CLEAR;
      return IRQ_HANDLED;
    }

    if ((cs & (BCM2835_SPI0_CS_RXF|BCM2835_SPI0_CS_RXR))) (void)spi->fifo;
    while (!(spi->cs & BCM2835_SPI0_CS_DONE))
    {
      if ((spi->cs & (BCM2835_SPI0_CS_RXF|BCM2835_SPI0_CS_RXR|BCM2835_SPI0_CS_RXD)))
        (void)spi->fifo;
    }
    CLEAR_GPIO(GPIO_TFT_DATA_CONTROL);
    spi->fifo = currentTask->cmd;
    if (currentTask->size == 0) // Was this a task without data bytes? If so, nothing more to do here, go to sleep to wait for next IRQ event
    {
      DoneTask((SPITask*)currentTask);
      taskNextByte = 0;
      currentTask = 0;
    }
    else
    {
      taskNextByte = currentTask->data;
      taskEndByte = currentTask->data + currentTask->size;
    }
#if 0 // Testing overhead of not returning after command byte, but synchronously polling it out..
    while (!(spi->cs & BCM2835_SPI0_CS_DONE)) ;
    (void)spi->fifo;
#else
    return IRQ_HANDLED;
#endif
  }
  if (taskNextByte == currentTask->data)
  {
    SET_GPIO(GPIO_TFT_DATA_CONTROL);
    __sync_synchronize();
  }

  // Test code: write and read from FIFO as many bytes as spec says we should be allowed to, without checking CS in between.
//  int maxBytesToSend = (cs & BCM2835_SPI0_CS_DONE) ? 16 : 12;
//  if ((cs & BCM2835_SPI0_CS_RXF)) (void)spi->fifo;
//  if ((cs & BCM2835_SPI0_CS_RXR)) for(int i = 0; i < MIN(maxBytesToSend, taskEndByte-taskNextByte); ++i) { spi->fifo = *taskNextByte++; (void)spi->fifo; }
//  else for(int i = 0; i < MIN(maxBytesToSend, taskEndByte-taskNextByte); ++i) { spi->fifo = *taskNextByte++; }

  while(taskNextByte < taskEndByte)
  {
    uint32_t cs = spi->cs;
    if ((cs & (BCM2835_SPI0_CS_RXR | BCM2835_SPI0_CS_RXF))) spi->cs = cs | BCM2835_SPI0_CS_CLEAR_RX;
    if ((cs & BCM2835_SPI0_CS_TXD)) spi->fifo = *taskNextByte++;
    if ((cs & BCM2835_SPI0_CS_RXD)) (void)spi->fifo;
    else break;
  }

  if (taskNextByte >= taskEndByte)
  {
    if ((cs & BCM2835_SPI0_CS_INTR)) spi->cs = (cs & ~BCM2835_SPI0_CS_INTR) | BCM2835_SPI0_CS_INTD;
    taskNextByte = 0;
  }
  else
  {
    if (!(cs & BCM2835_SPI0_CS_INTR)) spi->cs = (cs | BCM2835_SPI0_CS_INTR) & ~BCM2835_SPI0_CS_INTR;
  }
#endif
  return IRQ_HANDLED;
}
#endif

#define req(cnd) if (!(cnd)) { LOG("!!!%s!!!\n", #cnd);}

uint32_t virt_to_bus_address(volatile void *virtAddress)
{
  return (uint32_t)virt_to_phys((void*)virtAddress) | 0x40000000U;
}

volatile int shuttingDown = 0;
dma_addr_t spiTaskMemoryPhysical = 0;

#ifdef USE_DMA_TRANSFERS

void DMATest(void);

// Debug code to verify memory->memory streaming of DMA, no SPI peripheral interaction (remove this)
void DMATest()
{
  LOG("Testing DMA transfers");

  dma_addr_t dma_mem_phys = 0;
  void *dma_mem = dma_alloc_writecombine(0, SHARED_MEMORY_SIZE, &dma_mem_phys, GFP_KERNEL);
  LOG("Allocated DMA memory: mem: %p, phys: %p", dma_mem, (void*)dma_mem_phys);

  spiTaskMemory = (SharedMemory *)dma_mem;
  while(!shuttingDown)
  {
    msleep(100);
    static int ctr = 0;
    uint32_t base = (ctr++ * 34153) % SPI_QUEUE_SIZE;
    uint32_t size = 65;
    uint32_t base2 = base + size;
    if (base2 + size > SPI_QUEUE_SIZE) continue;

    memset((void*)spiTaskMemory->buffer, 0xCB, SPI_QUEUE_SIZE);

    uint8_t *src = (uint8_t *)(spiTaskMemory->buffer + base);
    src = (uint8_t *)((uintptr_t)src);
    for(int i = 0; i < size; ++i)
      src[i] = i;

    uint8_t *dst = (uint8_t *)(spiTaskMemory->buffer + base2);
    dst = (uint8_t *)((uintptr_t)dst);

#define TO_BUS(ptr) (( ((uint32_t)dma_mem_phys + ((uintptr_t)(ptr) - (uintptr_t)dma_mem))) | 0xC0000000U)

    volatile DMAChannelRegisterFile *dmaCh = dma+dmaTxChannel;
//    printk(KERN_INFO "CS: %x, cbAddr: %p, ti: %x, src: %p, dst: %p, len: %u, stride: %u, nextConBk: %p, debug: %x",
//      dmaCh->cs, (void*)dmaCh->cbAddr, dmaCh->cb.ti, (void*)dmaCh->cb.src, (void*)dmaCh->cb.dst, dmaCh->cb.len, dmaCh->cb.stride, (void*)dmaCh->cb.next, dmaCh->cb.debug);

    volatile DMAControlBlock *cb = &spiTaskMemory->cb[0].cb;
    req(((uintptr_t)cb) % 256 == 0);
    cb->ti = BCM2835_DMA_TI_SRC_INC | BCM2835_DMA_TI_DEST_INC;
    cb->src = TO_BUS(src);
    cb->dst = TO_BUS(dst);
    cb->len = size;
    cb->stride = 0;
    cb->next = 0;
    cb->debug = 0;
    cb->reserved = 0;
//    DumpCS(dmaCh->cs);
//    DumpDebug(dmaCh->cb.debug);
//    DumpTI(dmaCh->cb.ti);
    LOG("Waiting for transfer %d, src:%p(phys:%p) to dst:%p (phys:%p)", ctr, (void*)src, (void*)cb->src, (void*)dst, (void*)cb->dst);
    writel(TO_BUS(cb), &dmaCh->cbAddr);
    writel(BCM2835_DMA_CS_ACTIVE | BCM2835_DMA_CS_END | BCM2835_DMA_CS_INT | BCM2835_DMA_CS_WAIT_FOR_OUTSTANDING_WRITES | BCM2835_DMA_CS_SET_PRIORITY(0xF) | BCM2835_DMA_CS_SET_PANIC_PRIORITY(0xF), &dmaCh->cs);

    while((readl(&dmaCh->cs) & BCM2835_DMA_CS_ACTIVE) && !shuttingDown)
    {
      cpu_relax();
    }

    if (shuttingDown)
    {
      LOG("Module shutdown");
      spiTaskMemory = 0;
      return;
    }
    int errors = 0;
    for(int i = 0; i < size; ++i)
      if (dst[i] != src[i])
      {
        errors = true;
        break;
      }

    if (errors)
    {
      printk(KERN_INFO "CS: %x, cbAddr: %p, ti: %x, src: %p, dst: %p, len: %u, stride: %u, nextConBk: %p, debug: %x",
        dmaCh->cs, (void*)dmaCh->cbAddr, dmaCh->cb.ti, (void*)dmaCh->cb.src, (void*)dmaCh->cb.dst, dmaCh->cb.len, dmaCh->cb.stride, (void*)dmaCh->cb.next, dmaCh->cb.debug);
      for(int i = 0; i < size; ++i)
      {
        printk(KERN_INFO "Result %p %d: %x vs dst %p %x\n", (void*)virt_to_phys(src+i), i, src[i], (void*)virt_to_phys(dst+i), dst[i]);
      }
      DumpCS(dmaCh->cs);
      DumpDebug(dmaCh->cb.debug);
      DumpTI(dmaCh->cb.ti);
      LOG("Abort");
      break;
    }
  }
  LOG("DMA transfer test done");
  spiTaskMemory = 0;
}
#endif

void PumpSPI(void)
{
#ifdef KERNEL_DRIVE_WITH_IRQ
  spi->cs = BCM2835_SPI0_CS_CLEAR | BCM2835_SPI0_CS_TA | BCM2835_SPI0_CS_INTR | BCM2835_SPI0_CS_INTD; // Initialize the Control and Status register to defaults: CS=0 (Chip Select), CPHA=0 (Clock Phase), CPOL=0 (Clock Polarity), CSPOL=0 (Chip Select Polarity), TA=0 (Transfer not active), and reset TX and RX queues.
#else
  if (spiTaskMemory->queueTail != spiTaskMemory->queueHead)
  {
    BEGIN_SPI_COMMUNICATION();
    {
      int i = 0;
      while(spiTaskMemory->queueTail != spiTaskMemory->queueHead)
      {
        ++i;
        if (i > 500) break;
        SPITask *task = GetTask();
        if (task)
        {
          RunSPITask(task);
          DoneTask(task);
        }
        else
          break;
      }
    }
    END_SPI_COMMUNICATION();
  }
#endif
}

static struct timer_list my_timer;
 void my_timer_callback( unsigned long data )
{
  if (shuttingDown) return;

  PumpSPI();
  int ret = mod_timer( &my_timer, jiffies + msecs_to_jiffies(1) );
  if (ret) printk("Error in mod_timer\n");
}

static int display_initialization_thread(void *unused)
{
  printk(KERN_INFO "BCM2835 SPI Display driver thread started");

#ifndef KERNEL_MODULE_CLIENT_DRIVES

  // Initialize display. TODO: Move to be shared with ili9341.cpp.
  QUEUE_SPI_TRANSFER(0xC0/*Power Control 1*/, 0x23/*VRH=4.60V*/); // Set the GVDD level, which is a reference level for the VCOM level and the grayscale voltage level.
  QUEUE_SPI_TRANSFER(0xC1/*Power Control 2*/, 0x10/*AVCC=VCIx2,VGH=VCIx7,VGL=-VCIx4*/); // Sets the factor used in the step-up circuits. To reduce power consumption, set a smaller factor.
  QUEUE_SPI_TRANSFER(0xC5/*VCOM Control 1*/, 0x3e/*VCOMH=4.250V*/, 0x28/*VCOML=-1.500V*/); // Adjusting VCOM 1 and 2 can control display brightness
  QUEUE_SPI_TRANSFER(0xC7/*VCOM Control 2*/, 0x86/*VCOMH=VMH-58,VCOML=VML-58*/);

#define MADCTL_ROW_COLUMN_EXCHANGE (1<<5)
#define MADCTL_BGR_PIXEL_ORDER (1<<3)
#define MADCTL_ROTATE_180_DEGREES (MADCTL_COLUMN_ADDRESS_ORDER_SWAP | MADCTL_ROW_ADDRESS_ORDER_SWAP)
  uint8_t madctl = MADCTL_BGR_PIXEL_ORDER;
#ifdef DISPLAY_OUTPUT_LANDSCAPE
  madctl |= MADCTL_ROW_COLUMN_EXCHANGE;
#endif
#ifdef DISPLAY_ROTATE_180_DEGREES
    madctl ^= MADCTL_ROTATE_180_DEGREES;
#endif
  QUEUE_SPI_TRANSFER(0x36/*MADCTL: Memory Access Control*/, madctl);
  QUEUE_SPI_TRANSFER(0x3A/*COLMOD: Pixel Format Set*/, 0x55/*DPI=16bits/pixel,DBI=16bits/pixel*/);
  QUEUE_SPI_TRANSFER(0xB1/*Frame Rate Control (In Normal Mode/Full Colors)*/, 0x00/*DIVA=fosc*/, 0x18/*RTNA(Frame Rate)=79Hz*/);
  QUEUE_SPI_TRANSFER(0xB6/*Display Function Control*/, 0x08/*PTG=Interval Scan,PT=V63/V0/VCOML/VCOMH*/, 0x82/*REV=1(Normally white),ISC(Scan Cycle)=5 frames*/, 0x27/*LCD Driver Lines=320*/);
  QUEUE_SPI_TRANSFER(0x26/*Gamma Set*/, 0x01/*Gamma curve 1 (G2.2)*/);
  QUEUE_SPI_TRANSFER(0xE0/*Positive Gamma Correction*/, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00);
  QUEUE_SPI_TRANSFER(0xE1/*Negative Gamma Correction*/, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F);
  QUEUE_SPI_TRANSFER(0x11/*Sleep Out*/);

  PumpSPI();
  msleep(1000);
  QUEUE_SPI_TRANSFER(/*Display ON*/0x29);

#if 1
  // XXX Debug: Random garbage to verify screen updates working
  for(int y = 0; y < DISPLAY_HEIGHT; ++y)
  {
    QUEUE_SPI_TRANSFER(DISPLAY_SET_CURSOR_X, 0, 0, DISPLAY_WIDTH >> 8, DISPLAY_WIDTH & 0xFF);
    QUEUE_SPI_TRANSFER(DISPLAY_SET_CURSOR_Y, y >> 8, y & 0xFF, DISPLAY_HEIGHT >> 8, DISPLAY_HEIGHT & 0xFF);
    SPITask *clearLine = AllocTask(DISPLAY_SCANLINE_SIZE);
    clearLine->cmd = DISPLAY_WRITE_PIXELS;
    clearLine->size = DISPLAY_SCANLINE_SIZE;
    for(int i = 0; i < DISPLAY_SCANLINE_SIZE; ++i)
      clearLine->data[i] = tick() * y + i;
    CommitTask(clearLine);
  }
  PumpSPI();
  msleep(1000);
#endif

  // Initial screen clear
  for(int y = 0; y < DISPLAY_HEIGHT; ++y)
  {
    QUEUE_SPI_TRANSFER(DISPLAY_SET_CURSOR_X, 0, 0, DISPLAY_WIDTH >> 8, DISPLAY_WIDTH & 0xFF);
    QUEUE_SPI_TRANSFER(DISPLAY_SET_CURSOR_Y, y >> 8, y & 0xFF, DISPLAY_HEIGHT >> 8, DISPLAY_HEIGHT & 0xFF);
    SPITask *clearLine = AllocTask(DISPLAY_SCANLINE_SIZE);
    clearLine->cmd = DISPLAY_WRITE_PIXELS;
    clearLine->size = DISPLAY_SCANLINE_SIZE;
    memset((void*)clearLine->data, 0, DISPLAY_SCANLINE_SIZE);
    CommitTask(clearLine);
  }
  PumpSPI();

  QUEUE_SPI_TRANSFER(DISPLAY_SET_CURSOR_X, 0, 0, DISPLAY_WIDTH >> 8, DISPLAY_WIDTH & 0xFF);
  QUEUE_SPI_TRANSFER(DISPLAY_SET_CURSOR_Y, 0, 0, DISPLAY_HEIGHT >> 8, DISPLAY_HEIGHT & 0xFF);

  spi->cs = BCM2835_SPI0_CS_CLEAR | BCM2835_SPI0_CS_TA | BCM2835_SPI0_CS_INTR | BCM2835_SPI0_CS_INTD;
#endif

  PumpSPI();

  // Expose SPI worker ring bus to user space driver application.
  proc_create(SPI_BUS_PROC_ENTRY_FILENAME, 0, NULL, &fops);

#if 0
  // XXX Debug:
  DMATest();
#endif

  setup_timer(&my_timer, my_timer_callback, 0);
   printk("Starting timer to fire in 200ms (%ld)\n", jiffies);
  int ret = mod_timer( &my_timer, jiffies + msecs_to_jiffies(200) );
  if (ret) printk("Error in mod_timer\n");
 
  return 0;
}

static struct task_struct *displayThread = 0;
static uint32_t irqHandlerCookie = 0;
static uint32_t irqRegistered = 0;

int bcm2835_spi_display_init(void)
{
  InitSPI();
#ifdef KERNEL_DRIVE_WITH_IRQ
  int ret = request_irq(84, irq_handler, IRQF_SHARED, "spi_handler", &irqHandlerCookie);
  if (ret != 0) FATAL_ERROR("request_irq failed!");
  irqRegistered = 1;
#endif

  if (!spiTaskMemory) FATAL_ERROR("Shared memory block not initialized!");

#ifdef USE_DMA_TRANSFERS
  printk(KERN_INFO "DMA TX channel: %d, irq: %d", dmaTxChannel, dmaTxIrq);
  printk(KERN_INFO "DMA RX channel: %d, irq: %d", dmaRxChannel, dmaRxIrq);
  spiTaskMemory->dmaTxChannel = dmaTxChannel;
  spiTaskMemory->dmaRxChannel = dmaRxChannel;
#endif

  spiTaskMemory->sharedMemoryBaseInPhysMemory = (uint32_t)virt_to_phys(spiTaskMemory) | 0x40000000U;
  LOG("PhysBase: %p", (void*)spiTaskMemory->sharedMemoryBaseInPhysMemory);

  displayThread = kthread_create(display_initialization_thread, NULL, "display_thread");
  if (displayThread) wake_up_process(displayThread);

  return 0;
}

void bcm2835_spi_display_exit(void)
{
  shuttingDown = 1;
  msleep(2000);
  spi->cs = BCM2835_SPI0_CS_CLEAR;
  msleep(200);
  DeinitSPI();

  if (irqRegistered)
  {
    free_irq(84, &irqHandlerCookie);
    irqRegistered = 0;
  }

  remove_proc_entry(SPI_BUS_PROC_ENTRY_FILENAME, NULL);

  int ret = del_timer( &my_timer );
  if (ret) printk("The timer is still in use...\n");}

module_init(bcm2835_spi_display_init);
module_exit(bcm2835_spi_display_exit);
