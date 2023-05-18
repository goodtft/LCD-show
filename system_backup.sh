#!/bin/bash
if [ ! -d "./.system_backup" ]; then
sudo mkdir ./.system_backup
else
    echo "It looks like a backup of your previous configuration already exists. Would you like to delete that backup and create a new one of the current configuration? [Y]es or [N]o."
    read override_choice
    if [ "${override_choice^^}" = "N" ]; then
        echo "Not overwriting existing backup and cancelling backup process!"
        exit
    else
        echo "Overwriting existing backup to create a new one now."
    fi
fi

sudo rm -rf ./.system_backup/*

if [ -f /etc/X11/xorg.conf.d/99-calibration.conf ]; then
sudo cp -rf /etc/X11/xorg.conf.d/99-calibration.conf ./.system_backup
sudo rm -rf /etc/X11/xorg.conf.d/99-calibration.conf
fi

if [ -f /etc/X11/xorg.conf.d/40-libinput.conf ]; then
sudo cp -rf /etc/X11/xorg.conf.d/40-libinput.conf ./.system_backup
sudo rm -rf /etc/X11/xorg.conf.d/40-libinput.conf
fi

if [ -d /etc/X11/xorg.conf.d ]; then
sudo mkdir -p ./.system_backup/xorg.conf.d
sudo cp -r /etc/X11/xorg.conf.d/* ./.system_backup/xorg.conf.d
sudo rm -rf /etc/X11/xorg.conf.d
fi

result=`grep -rn "^dtoverlay=" /boot/config.txt | grep ":rotate=" | tail -n 1`
if [ $? -eq 0 ]; then
str=`echo -n $result | awk -F: '{printf $2}' | awk -F= '{printf $NF}'`
if [ -f /boot/overlays/$str-overlay.dtb ]; then
sudo cp -rf /boot/overlays/$str-overlay.dtb ./.system_backup
sudo rm -rf /boot/overlays/$str-overlay.dtb
fi
if [ -f /boot/overlays/$str.dtbo ]; then
sudo cp -rf /boot/overlays/$str.dtbo ./.system_backup
sudo rm -rf /boot/overlays/$str.dtbo
fi
fi

root_dev=`grep -oPr "root=[^\s]*" /boot/cmdline.txt | awk -F= '{printf $NF}'`
sudo cp -rf /boot/config.txt ./.system_backup
sudo cp -rf /boot/cmdline.txt ./.system_backup/
if [ -f /usr/share/X11/xorg.conf.d/99-fbturbo.conf ]; then
sudo cp -rf /usr/share/X11/xorg.conf.d/99-fbturbo.conf ./.system_backup/
fi
#sudo cp -rf ./usr/99-fbturbo.conf-original /usr/share/X11/xorg.conf.d/99-fbturbo.conf
if [ -f /etc/rc.local ]; then
sudo cp -rf /etc/rc.local ./.system_backup/
#sudo cp -rf ./etc/rc.local-original /etc/rc.local
fi

if [ -f /etc/modules ]; then
sudo cp -rf /etc/modules ./.system_backup/
#sudo cp -rf ./etc/modules-original /etc/modules
fi

if [ -f /etc/modprobe.d/fbtft.conf ]; then
sudo cp -rf /etc/modprobe.d/fbtft.conf ./.system_backup
sudo rm -rf /etc/modprobe.d/fbtft.conf
fi

if [ -f /etc/inittab ]; then
sudo cp -rf /etc/inittab ./.system_backup
sudo rm -rf /etc/inittab
fi

type fbcp > /dev/null 2>&1
if [ $? -eq 0 ]; then
sudo touch ./.system_backup/have_fbcp
sudo rm -rf /usr/local/bin/fbcp
fi

#type cmake > /dev/null 2>&1
#if [ $? -eq 0 ]; then
#sudo touch ./.system_backup/have_cmake
#sudo apt-get purge cmake -y 2> error_output.txt
#result=`cat ./error_output.txt`
#echo -e "\033[31m$result\033[0m"
#fi

if [ -f /usr/share/X11/xorg.conf.d/10-evdev.conf ]; then
sudo cp -rf /usr/share/X11/xorg.conf.d/10-evdev.conf ./.system_backup
#sudo dpkg -P xserver-xorg-input-evdev
#sudo apt-get purge xserver-xorg-input-evdev -y  2> error_output.txt
#result=`cat ./error_output.txt`
#echo -e "\033[31m$result\033[0m"
fi

if [ -f /usr/share/X11/xorg.conf.d/45-evdev.conf ]; then
sudo cp -rf /usr/share/X11/xorg.conf.d/45-evdev.conf ./.system_backup
sudo rm -rf /usr/share/X11/xorg.conf.d/45-evdev.conf
fi

if [ -f ./.have_installed ]; then
sudo cp -rf ./.have_installed ./.system_backup
sudo rm -rf ./.have_installed
fi
