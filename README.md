#LCD driver for the Raspberry PI Installation

##Update: 
  v1.2-20170302
  ##dd the command bellow to solve problem with Raspbian-2017-03-02<br><br>
```sudo dpkg -i -B xserver-xorg-input-evdev_1%3a2.10.3-1_armhf.deb```<br><br>
```sudo cp -rf /usr/share/X11/xorg.conf.d/10-evdev.conf /usr/share/X11/xorg.conf.d/45-evdev.conf```<br><br>

##Update: 
  v1.1-20160815
  
##1.)Step1, Install Raspbian official mirror  
  a)Download Raspbian official mirror:
  https://www.raspberrypi.org/downloads/<br>
  b)Use“SDFormatter.exe”to Format your TF Card<br>
  c)Use“Win32DiskImager.exe” Burning mirror to TF Card<br>
     
##2.) Clone my repo onto your pi
```git clone https://github.com/goodtft/LCD-show.git```<br><br>
```chmod -R 755 LCD-show```<br><br>
```cd LCD-show/```<br>
  
##3.)According to your LCD's type, excute:
###In case of 2.8" LCD
  ```sudo ./LCD28-show```
###In case of 3.2" LCD
  ```sudo ./LCD32-show```
###In case of 3.5" LCD
  ```sudo ./LCD35-show```
###In case of 3.97" LCD
  ```sudo ./LCD397-show```
###In case of 4.3" LCD
  ```sudo ./LCD43-show```
###In case of 5" LCD
  ```sudo ./LCD5-show```
###In case of 7inch(B)-800X480 RPI LCD
  ```sudo ./LCD7B-show```
###In case of 7inch(C)-1024X600 RPI LCD
  ```sudo ./LCD7C-show```
###If you need to switch back to the traditional HDMI display
  ```sudo ./LCD-hdmi```

##4.) Wait a few minutes,the system will restart automaticall , enjoy with your LCD.

