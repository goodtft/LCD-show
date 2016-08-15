
[version]
 v1.1
[Driver installation]
Step1, Install Raspbian official mirror                                                                     
1)Download Raspbian official mirror:https://www.raspberrypi.org/downloads/
2)Use¡°SDFormatter.exe¡±to Format your TF Card£¬
3)Use¡°Win32DiskImager.exe¡± Burning mirror to TF Card¡£
Step2, Install LCD Driver                                                                                
1)Copy ¡°LCD-show-160701.tar.gz¡± to the root directory of raspberry pi£¨you can copy it directly to TF card after Step1, or use SFTP to remote copy£©
2)Landing Raspberry pi system to user command line (Name:pi,Password:raspberry)£¬Execute the following command:
cd /boot
sudo tar zxvf LCD-show-160701.tar.gz
cd LCD-show/
#For 2.8inch RPI LCD excute:
sudo ./LCD28-show
# For 3.2inch RPI LCD excute:
sudo ./LCD32-show
# For 3.5inch RPI LCD excute:
sudo ./LCD35-show
# For 3.97inch RPI LCD excute:
sudo ./LCD397-show
# For 4.3inch RPI LCD excute:
sudo ./LCD43-show
# For 5inch RPI LCD excute:
sudo ./LCD5-show
# For 7inch(B)-800X480 RPI LCD excute:
sudo ./LCD7B-show
# For 7inch(C)-1024X600 RPI LCD excute:
sudo ./LCD7C-show
# If you need to switch back to the traditional HDMI display excute:
Sudo ./LCD-hdmi

3)Wait a few minutes,the system will restart automaticall , enjoy with your LCD.
