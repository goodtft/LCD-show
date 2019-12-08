# LCD Show

LCD show are the display drivers and scripts for 2.4, 2.8, 3.2, 3.5, 5.0 and 7.0 inch TFT LCD display for the Raspberry Pi 3B+/A/A+/B/B+/PI2/ PI3/ZERO/ZERO W.

Links for Ubuntu, Kali and Retropie Linux are available at the following locations.

* https://github.com/lcdwiki/LCD-show-ubuntu
* https://github.com/lcdwiki/LCD-show-kali
* https://github.com/lcdwiki/LCD-show-retropie

# Install the LCD driver of Raspberry Pi
  
## Step 1, Install Raspbian official mirror

1. Download Raspbian official mirror at https://www.raspberrypi.org/downloads/
2. Use `SDFormatter.exe` to format your memory card
3. Use `Win32DiskImager.exe` to burn image to memory card
     
## Step 2, Clone LCD-show repo onto your RPi

1. Use SSH to connect the Raspberry Pi
2. Ensure that the Raspberry Pi is connected to the Internet before executing the following commands:

```
sudo rm -rf LCD-show
git clone https://github.com/goodtft/LCD-show.git
chmod -R 755 LCD-show
cd LCD-show/
```

## Step 3, Execute the corresponding driver for your device

## 2.4 inch RPi Display (MPI2401):
#### Driver install:
* `sudo ./LCD24-show`
#### WIKI:
* CN: http://www.lcdwiki.com/zh/2.4inch_RPi_Display
* EN: http://www.lcdwiki.com/2.4inch_RPi_Display
 
## 2.4 inch RPi Display For RPi 3A+ (MPI2411):
#### Driver install:
* `sudo ./LCD24-3A+-show`  
#### WIKI:
* CN: http://www.lcdwiki.com/zh/2.4inch_RPi_Display_For_RPi_3A+
* EN: http://www.lcdwiki.com/2.4inch_RPi_Display_For_RPi_3A+

## 2.8 inch RPi Display (MPI2801):
#### Driver install:
* `sudo ./LCD28-show`
#### WIKI:
* CN: http://www.lcdwiki.com/zh/2.8inch_RPi_Display
* EN: http://www.lcdwiki.com/2.8inch_RPi_Display
  
## 3.2 inch RPi Display (MPI3201):
#### Driver install:
* `sudo ./LCD32-show`   
#### WIKI:
* CN: http://www.lcdwiki.com/zh/3.2inch_RPi_Display
* EN: http://www.lcdwiki.com/3.2inch_RPi_Display

## MHS-3.2 inch RPi Display (MHS3232):
#### Driver install:
* `sudo ./MHS32-show`   
#### WIKI:
* CN: http://www.lcdwiki.com/zh/MHS-3.2inch_Display
* EN: http://www.lcdwiki.com/MHS-3.2inch_Display

## 3.5 inch RPi Display (MPI3501):
#### Driver install:
* `sudo ./LCD35-show`
#### WIKI:
* CN: http://www.lcdwiki.com/zh/3.5inch_RPi_Display
* EN: http://www.lcdwiki.com/3.5inch_RPi_Display
   
## 3.5 inch HDMI Display-B (MPI3508):
#### Driver install:
* `sudo ./MPI3508-show`
#### WIKI:
* CN: http://www.lcdwiki.com/zh/3.5inch_HDMI_Display-B
* EN: http://www.lcdwiki.com/3.5inch_HDMI_Display-B
    
## MHS-3.5 inch RPi Display (MHS3528):
#### Driver install:
* `sudo ./MHS35-show`
#### WIKI:
* CN: http://www.lcdwiki.com/zh/MHS-3.5inch_RPi_Display
EN:http://www.lcdwiki.com/MHS-3.5inch_RPi_Display

## MHS-3.5 inch RPi Display-B (MHS35XX):
#### Driver install:
* `sudo ./MHS35B-show`
#### WIKI:
* CN: http://www.lcdwiki.com/zh/MHS-3.5inch_RPi_Display-B
EN:http://www.lcdwiki.com/MHS-3.5inch_RPi_Display-B

## 4.0 inch HDMI Display (MPI4008):
#### Driver install:
* `sudo ./MPI4008-show`
#### WIKI:
* CN: http://www.lcdwiki.com/zh/4inch_HDMI_Display-C
* EN: http://www.lcdwiki.com/4inch_HDMI_Display-C
   
## MHS-4.0 inch HDMI Display-B (MHS4001):
#### Driver install:
* `sudo ./MHS40-show`
#### WIKI:
* CN: http://www.lcdwiki.com/zh/MHS-4.0inch_Display-B
* EN: http://www.lcdwiki.com/MHS-4.0inch_Display-B
  
## 5.0 inch HDMI Display(Resistance touch) (MPI5008):
#### Driver install:
* `sudo ./LCD5-show`
#### WIKI:
* CN: http://www.lcdwiki.com/zh/5inch_HDMI_Display
* EN: http://www.lcdwiki.com/5inch_HDMI_Display
    
## 5 inch HDMI Display-B(Capacitor touch) (MPI5001):
#### Driver install:
* `sudo ./MPI5001-show`
#### WIKI:
* CN: http://www.lcdwiki.com/zh/5inch_HDMI_Display-B
* EN: http://www.lcdwiki.com/5inch_HDMI_Display-B
    
## 7 inch HDMI Display-B-800X480 (MPI7001):
#### Driver install:
* `sudo ./LCD7B-show`
#### WIKI:
* CN: http://www.lcdwiki.com/zh/7inch_HDMI_Display-B
* EN: http://www.lcdwiki.com/7inch_HDMI_Display-B
   
## 7 inch HDMI Display-C-1024X600 (MPI7002):
#### Driver install:
* `sudo ./LCD7C-show`
#### WIKI:
* CN: http://www.lcdwiki.com/zh/7inch_HDMI_Display-C
* EN: http://www.lcdwiki.com/7inch_HDMI_Display-C
   
Wait a moment after executing the above command, then you can use the corresponding raspberry LCD.

# How to rotate the display direction

This method only applies to the Raspberry Pi series of display screens, other display screens do not apply.

## Method 1, If the driver is not installed

Execute the following command if the driver is not installed. The Raspberry Pi needs an Internet connection.

```
sudo rm -rf LCD-show`
git clone https://github.com/goodtft/LCD-show`.git
chmod -R 755 LCD-show`
cd LCD-show`/
sudo ./XXX-show` 90
```

After execution, the driver will be installed. The system will automatically restart, and the display screen will rotate 90 degrees to display and touch normally.

`XXX-show` can be changed to the corresponding driver, and `90` can be changed to `0`, `90`, `180` or `270`, representing rotation angles of 0 degrees, 90 degrees, 180 degrees, 270 degrees, respecitively.

### Method 2, If the driver is already installed

Execute the following command if the driver is already installed. No Internet connection is required.

```
cd LCD-show/
sudo ./rotate.sh 90
```

After execution, the system will automatically restart and the driver will be installed. The system will automatically restart, and the display screen will rotate 90 degrees to display and touch normally.

`XXX-show` can be changed to the corresponding driver, and `90` can be changed to `0`, `90`, `180` or `270`, representing rotation angles of 0 degrees, 90 degrees, 180 degrees, 270 degrees, respecitively.

# Change Log
* v2.1-20191106
  - Update to support MHS35B
* v2.0-20190704
  - Update to support rotate the display direction
* v1.9-20181204
  - Update to support MHS40 & MHS32
* v1.8-20180907
  - Update to support MHS35
* v1.7-20180320
  - Update to support Raspbian Version: March 2018(Release date:2018-03-13)
* v1.6-20170824
  - Update xserver to support Raspbian-2017-08-16
* v1.5-20170706
  - Update to support Raspbian-2017-07-05, Raspbian-2017-06-21
* v1.3-20170612
  * fixed to support Raspbian-2017-03-02, Raspbian-2017-04-10
* v1.2-20170302
  * Add xserver-xorg-input-evdev_1%3a2.10.3-1_armhf.deb to support Raspbian-2017-03-02
* v1.1-20160815
