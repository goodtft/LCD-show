sudo ./stop_kernel_module.sh
sudo make -C /lib/modules/$(uname -r)/build M=$(pwd) modules

#For debugging: generate disassembly output:
#objdump -dS bcm2835_spi_display.ko > bcm2835_spi_display.S

