LCD driver for the Raspberry PI Installation<br>
====================================================
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

{| class="wikitable" border="1" style="width: 80%; margin-left:1.5%;background-color: white;"
!系统名称
!系统版本
!支持的树莓派版本
!默认密码
! colspan="2" |下载地址
|-
| rowspan="2" |Raspbian
| rowspan="2" |2018-06-29
| rowspan="2" |PI3B+/A/A+/B/B+/PI2/
PI3/ZERO/ZERO W
| rowspan="2" |user:pi
password:raspberry
|(addr1)Baidu Pan:
|[https://pan.baidu.com/s/1GovMkvIRtnssAHhooGJWQg MPI5001-B-800x480-Raspbian-20180629.7z]
|-
|(addr2)Mega:
|[https://mega.nz/#!3HZSGYbC!X3NK1mgBffsvfaqFRbWFsWpaOeB7NvYbmxNIpcYmHXs MPI5001-B-800x480-Raspbian-20180629.7z]
|-
| rowspan="2" |Ubuntu
| rowspan="2" |Mate:16.04
| rowspan="2" |PI3,PI2
| rowspan="2" |user:pi
password:raspberry
|(addr1)Baidu Pan:
|[https://pan.baidu.com/s/1otoEtJ8jBBRG9cqDQhdP_g 5inchB-800X480-RPI3-RPI2-ubuntu-mate-16.04-beta2.7z]
|-
|(addr2)Mega:
|[https://mega.nz/#!LHYyCI7B!BbR3ykwkkBClbH0Ie-XmboQNY8pgDuYmm9YETfSSF4s 5inchB-800X480-RPI3-RPI2-ubuntu-mate-16.04-beta2.7z]
|-
| rowspan="2" |Kali-linux
| rowspan="2" |2018.2 nexmon
| rowspan="2" |PI3B+,PI3,PI2
| rowspan="2" |user:root
password:toor
|(addr1)Baidu Pan:
|[https://pan.baidu.com/s/1atq_CuHOIQjPoYskulC5xw MPI5001-800X480-kali-linux-2018.2-rpi3-nexmon.7z]
|-
|(addr2)Mega:
|[https://mega.nz/#!mToARSSB!D7aKYEKVAhO0Vok3fRqGA2RNmHRY2EvGqWQGfs0FEDI MPI5001-800X480-kali-linux-2018.2-rpi3-nexmon.7z]
|}

