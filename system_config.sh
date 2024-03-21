#!/bin/bash

big_version=`lsb_release -r | awk -F ' '  '{printf $NF}'`
deb_version=`cat /etc/debian_version | tr -d '\n'`
hw_result=`tr -d '\0' < /proc/device-tree/model`

if [ $(getconf WORD_BIT) = '32' ] && [ $(getconf LONG_BIT) = '64' ] ; then
hardware_arch=64
else
hardware_arch=32
fi

if [[ $hw_result == *"Raspberry Pi 5"* ]]; then
hardware_model=5
else
hardware_model=255
fi

sudo raspi-config nonint do_wayland W1
if [ -f /boot/firmware/config.txt ]; then
sudo ln -sf /boot/firmware/config.txt /boot/config.txt
fi

if [ $hardware_arch -eq 32 ]; then
if [ $(($big_version)) -lt 10 ]; then
sudo cp -rf ./boot/config-nomal-10.9-32.txt ./boot/config.txt.bak
else
if [[ "$deb_version" < "10.9" ]] || [[ "$deb_version" = "10.9" ]]; then
sudo cp -rf ./boot/config-nomal-10.9-32.txt ./boot/config.txt.bak
elif [[ "$deb_version" < "12.1" ]]; then
sudo cp -rf ./boot/config-nomal-11.4-32.txt ./boot/config.txt.bak
else
sudo cp -rf ./boot/config-nomal-12.1-32.txt ./boot/config.txt.bak
fi
fi
elif [ $hardware_arch -eq 64 ]; then
sudo cp -rf ./boot/config-nomal-11.4-64.txt ./boot/config.txt.bak
fi


