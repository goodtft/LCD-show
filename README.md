LCD driver for the Raspberry PI Installation<br>
====================================================
Update: <br>
v1.9-20181204<br>
Update to support MHS40 & MHS32<br>
Update: <br>
v1.8-20180907<br>
Update to support MHS35<br>
Update: <br>
v1.7-20180320<br>
Update to support Raspbian Version:March 2018(Release date:2018-03-13)<br>
Update: <br>
  v1.6-20170824<br>
  Update xserver to support Raspbian-2017-08-16<br>
Update: <br>
  v1.5-20170706<br>
  Update to support Raspbian-2017-07-05,Raspbian-2017-06-21<br>
Update: <br>
  v1.3-20170612<br>
  fixed to support Raspbian-2017-03-02,Raspbian-2017-04-10<br>
Update: <br>
  v1.2-20170302<br>
  Add xserver-xorg-input-evdev_1%3a2.10.3-1_armhf.deb to support Raspbian-2017-03-02<br>
Update: <br>
  v1.1-20160815<br><br>
  
1.)Step1, Install Raspbian official mirror <br>
====================================================
  a)Download Raspbian official mirror:<br>
  https://www.raspberrypi.org/downloads/<br>
  b)Use“SDFormatter.exe”to Format your TF Card<br>
  c)Use“Win32DiskImager.exe” Burning mirror to TF Card<br>
     
2.) Step2, Clone my repo onto your pi<br>
====================================================
Use SSH to connect the raspberry pi, <br>
And Ensure that the raspberry pi is connected to the Internet before executing the following commands:
-----------------------------------------------------------------------------------------------------

```sudo rm -rf LCD-show```<br>
```git clone https://github.com/goodtft/LCD-show.git```<br>
```chmod -R 755 LCD-show```<br>
```cd LCD-show/```<br>
  
3.)Step3, According to your LCD's type, excute:
====================================================
In case of 2.4" RPi Display(MPI2401)<br>
  ```sudo ./LCD24-show```<br><br>
In case of 2.8" RPi Display(MPI2801)<br>
  ```sudo ./LCD28-show```<br><br>
In case of 3.2" RPi Display(MPI3201)<br>
  ```sudo ./LCD32-show```<br><br>
In case of 3.5inch RPi Display(MPI3501)<br>
  ```sudo ./LCD35-show```<br><br>
In case of 3.5" HDMI Display-B(MPI3508)<br>
  ```sudo ./MPI3508-show```<br><br>
 In case of 3.2" High Speed display(MHS32)<br>
  ```sudo ./MHS32-show```<br><br>
In case of 3.5" High Speed display(MHS35)<br>
  ```sudo ./MHS35-show```<br><br>
In case of 4.0" High Speed display(MHS40)<br>
  ```sudo ./MHS40-show```<br><br>
In case of 4.0" HDMI Display(MPI4008)<br>
  ```sudo ./MPI4008-show```<br><br>
In case of 5inch HDMI Display-B(Capacitor touch)(MPI5001):<br>
  ```sudo ./MPI5001-show```<br><br>  
In case of 5inch HDMI Display(Resistance touch)(MPI5008)<br>
  ```sudo ./LCD5-show```<br><br>
In case of 7inch HDMI Display-B-800X480(MPI7001)<br>
  ```sudo ./LCD7B-show```<br><br>
In case of 7inch HDMI Display-C-1024X600(MPI7002)<br>
  ```sudo ./LCD7C-show```<br><br><br>
If you need to switch back to the traditional HDMI display<br>
  ```sudo ./LCD-hdmi```<br>

Wait a few minutes,the system will restart automaticall , enjoy with your LCD.
-------------------------------------------------------------------------------
The LCD-show.tar.gz also can be download from:
http://www.lcdwiki.com/RaspberryPi-LCD-Driver
<br><br>
