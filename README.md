LCD driver for the Raspberry PI Installation<br>
====================================================

Update: <br><br>
-----------------------------------------------------
  v1.2-20170302<br><br>
  Add the command bellow to solve problem with Raspbian-2017-03-02<br><br>
```sudo dpkg -i -B xserver-xorg-input-evdev_1%3a2.10.3-1_armhf.deb```<br><br>
```sudo cp -rf /usr/share/X11/xorg.conf.d/10-evdev.conf /usr/share/X11/xorg.conf.d/45-evdev.conf```<br><br>

Update: <br><br>
-----------------------------------------------------
  v1.1-20160815<br><br>
  
1.)Step1, Install Raspbian official mirror <br><br> 
====================================================
  a)Download Raspbian official mirror:<br><br>
  https://www.raspberrypi.org/downloads/<br>
  b)Use“SDFormatter.exe”to Format your TF Card<br>
  c)Use“Win32DiskImager.exe” Burning mirror to TF Card<br>
     
2.) Clone my repo onto your pi<br><br>
====================================================
```git clone https://github.com/goodtft/LCD-show.git```<br>
```chmod -R 755 LCD-show```<br>
```cd LCD-show/```<br>
  
3.)According to your LCD's type, excute:
====================================================
In case of 2.8" LCD<br><br>
  ```sudo ./LCD28-show```<br><br>
In case of 3.2" LCD<br><br>
  ```sudo ./LCD32-show```<br><br>
In case of 3.5" LCD<br><br>
  ```sudo ./LCD35-show```<br><br>
In case of 3.97" LCD<br><br>
  ```sudo ./LCD397-show```<br><br>
In case of 4.3" LCD<br><br>
  ```sudo ./LCD43-show```<br><br>
In case of 5" LCD<br><br>
  ```sudo ./LCD5-show```<br><br>
In case of 7inch(B)-800X480 RPI LCD<br><br>
  ```sudo ./LCD7B-show```<br><br>
In case of 7inch(C)-1024X600 RPI LCD<br><br>
  ```sudo ./LCD7C-show```<br><br>
If you need to switch back to the traditional HDMI display<br><br>
  ```sudo ./LCD-hdmi```<br><br>

4.) Wait a few minutes,the system will restart automaticall , enjoy with your LCD.
======================================================================================

