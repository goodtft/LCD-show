#!/bin/bash

big_version=`lsb_release -r | awk -F ' '  '{printf $NF}'`
deb_version=`cat /etc/debian_version | tr -d '\n'`

if [ $(getconf WORD_BIT) = '32' ] && [ $(getconf LONG_BIT) = '64' ] ; then
hardware_arch=64
else
hardware_arch=32
fi

if [ $hardware_arch -eq 32 ]; then
if [ $(($big_version)) -lt 10 ]; then
sudo cp -rf ./boot/config-nomal-10.9-32.txt ./boot/config.txt.bak
else
if [[ "$deb_version" < "10.9" ]] || [[ "$deb_version" = "10.9" ]]; then
sudo cp -rf ./boot/config-nomal-10.9-32.txt ./boot/config.txt.bak
else
sudo cp -rf ./boot/config-nomal-11.4-32.txt ./boot/config.txt.bak
fi
fi
elif [ $hardware_arch -eq 64 ]; then
sudo cp -rf ./boot/config-nomal-11.4-64.txt ./boot/config.txt.bak
fi


