
[version]
 v1.1
[Driver installation]
Step1, Install Raspbian official mirror                                                                     
1)Download Raspbian official mirror:https://www.raspberrypi.org/downloads/
2)Use“SDFormatter.exe”to Format your TF Card，
3)Use“Win32DiskImager.exe” Burning mirror to TF Card。
Step2, Install LCD Driver                                                                                
1)Clone my repo onto your pi
git clone https://github.com/goodtft/LCD-show.git
cd LCD-show/
#For 2.8inch RPI LCD excute:
chmod 755 LCD28-show
sudo ./LCD28-show
# For 3.2inch RPI LCD excute:
chmod 755 LCD32-show
sudo ./LCD32-show
# For 3.5inch RPI LCD excute:
chmod 755 LCD35-show
sudo ./LCD35-show
# For 3.97inch RPI LCD excute:
chmod 755 LCD397-show
sudo ./LCD397-show
# For 4.3inch RPI LCD excute:
chmod 755 LCD43-show
sudo ./LCD43-show
# For 5inch RPI LCD excute:
chmod 755 LCD5-show
sudo ./LCD5-show
# For 7inch(B)-800X480 RPI LCD excute:
chmod 755 LCD7B-show
sudo ./LCD7B-show
# For 7inch(C)-1024X600 RPI LCD excute:
chmod 755 LCD7C-show
sudo ./LCD7C-show
# If you need to switch back to the traditional HDMI display excute:
chmod 755 LCD-hdmi
Sudo ./LCD-hdmi

3)Wait a few minutes,the system will restart automaticall , enjoy with your LCD.
