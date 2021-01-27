# Kill user space driver program first if it happens to be running, because otherwise shutting down the kernel
# module would crash the system if the userland program was still accessing it.
echo Killing existing instances of user space driver program fbcp-ili9341
sudo pkill fbcp-ili9341
sudo pkill fbcp-ili9341-stable

# Now safe to tear down the module
echo Stopping kernel module bcm2835_spi_display.ko
sudo rmmod bcm2835_spi_display.ko

