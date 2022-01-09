#!/bin/bash
cur_dir=`pwd`
if [ ! -f $cur_dir/.have_installed ]; then
echo "Please install the LCD driver first"
echo "Usage: sudo ./xxx-show. xxx: MHS35,LCD35,MPI3508 etc."
exit
fi

print_info()
{
echo "Usage:sudo ./rotate.sh [0] [90] [180] [270] [360] [450]"
echo "0-Screen rotation 0 degrees"
echo "90-Screen rotation 90 degrees"
echo "180-Screen rotation 180 degrees"
echo "270-Screen rotation 270 degrees"
echo "360-Screen flip horizontal(Valid only for HDMI screens)"
echo "450-Screen flip vertical(Valid only for HDMI screens)"
}

if [ $# -eq 0 ]; then
echo "Please input parameter:0,90,180,270,360,450"
print_info
exit
elif [ $# -eq 1 ]; then
if [ ! -n "$(echo $1| sed -n "/^[0-9]\+$/p")" ]; then
echo "Invalid parameter"
print_info
exit
else
if [ $1 -ne 0 ] && [ $1 -ne 90 ] && [ $1 -ne 180 ] && [ $1 -ne 270 ] && [ $1 -ne 360 ] && [ $1 -ne 450 ]; then
echo "Invalid parameter"
print_info
exit
fi
fi
else
echo "Too many parameters, only one parameter allowed"
exit
fi

#get screen parameter
tmp=`cat $cur_dir/.have_installed`
output_type=`cat $cur_dir/.have_installed | awk -F ':' '{printf $1}'`
touch_type=`cat $cur_dir/.have_installed | awk -F ':' '{printf $2}'`
device_id=`cat $cur_dir/.have_installed | awk -F ':' '{printf $3}'`
default_value=`cat $cur_dir/.have_installed | awk -F ':' '{printf $4}'`
width=`cat $cur_dir/.have_installed | awk -F ':' '{printf $5}'`
height=`cat $cur_dir/.have_installed | awk -F ':' '{printf $6}'`

if [ $output_type = "hdmi" ]; then
result=`grep -rn "^display_rotate=" /boot/config.txt | tail -n 1`
line=`echo -n $result | awk -F: '{printf $1}'`
str=`echo -n $result | awk -F: '{printf $NF}'`
old_rotate_value=`echo -n $result | awk -F= '{printf $NF}'`
if [ $old_rotate_value = "0x10000" ]; then
old_rotate_value=4
elif  [ $old_rotate_value = "0x20000" ]; then
old_rotate_value=5
fi
if [ $1 -eq 0 ] || [ $1 -eq 90 ] || [ $1 -eq 180 ] || [ $1 -eq 270 ]; then
new_rotate_value=$[(($default_value+$1)%360)/90]
else
new_rotate_value=$[$1/90]
fi
elif [ $output_type = "gpio" ]; then
result=`grep -rn "^dtoverlay=" /boot/config.txt | grep ":rotate=" | tail -n 1`
line=`echo -n $result | awk -F: '{printf $1}'`
str=`echo -n $result | awk -F: '{printf $NF}'`
old_rotate_value=`echo -n $result | awk -F= '{printf $NF}'`
if [ $1 -eq 0 ] || [ $1 -eq 90 ] || [ $1 -eq 180 ] || [ $1 -eq 270 ]; then
new_rotate_value=$[($default_value+$1)%360]
else
echo "Invalid parameter: only for HDMI screens"
exit
fi
else
echo "Invalid output type"
exit
fi

if [ $old_rotate_value -eq $new_rotate_value ]; then
if [ $output_type = "hdmi" ]; then
if [ $1 -eq 0 ] || [ $1 -eq 90 ] || [ $1 -eq 180 ] || [ $1 -eq 270 ]; then
old_rotate_value=$[($old_rotate_value*90+360-$default_value)%360]
else
old_rotate_value=$[$old_rotate_value*90]
fi
elif [ $output_type = "gpio" ]; then
old_rotate_value=$[($old_rotate_value+360-$default_value)%360]
fi
echo "Current rotate value is $old_rotate_value"
exit
fi

#setting LCD rotate
if [ $output_type = "hdmi" ]; then
if [ $new_rotate_value -eq 4 ]; then
sudo sed -i -e ''"$line"'s/'"$str"'/display_rotate=0x10000/' /boot/config.txt
elif  [ $new_rotate_value -eq 5 ]; then
sudo sed -i -e ''"$line"'s/'"$str"'/display_rotate=0x20000/' /boot/config.txt
else
sudo sed -i -e ''"$line"'s/'"$str"'/display_rotate='"$new_rotate_value"'/' /boot/config.txt
fi
new_rotate_value=$[$new_rotate_value*90]
elif [ $output_type = "gpio" ]; then
sudo sed -i -e ''"$line"'s/'"$str"'/rotate='"$new_rotate_value"'/' /boot/config.txt
resultr=`grep -rn "^hdmi_cvt" /boot/config.txt | tail -n 1 | awk -F' ' '{print $1,$2,$3}'`
if [ -n "$resultr" ]; then
liner=`echo -n $resultr | awk -F: '{printf $1}'`
strr=`echo -n $resultr | awk -F: '{printf $2}'`
if [ $new_rotate_value -eq $default_value ] || [ $new_rotate_value -eq $[($default_value+180+360)%360] ]; then
sudo sed -i -e ''"$liner"'s/'"$strr"'/hdmi_cvt '"$width"' '"$height"'/' /boot/config.txt
elif [ $new_rotate_value -eq $[($default_value-90+360)%360] ] || [ $new_rotate_value -eq $[($default_value+90+360)%360] ]; then
sudo sed -i -e ''"$liner"'s/'"$strr"'/hdmi_cvt '"$height"' '"$width"'/' /boot/config.txt
fi
fi
fi

#setting touch screen rotate
if [ $touch_type = "resistance" ]; then 
if [ $new_rotate_value -eq 0 ]; then
sudo cp $cur_dir/usr/99-calibration.conf-$device_id-0 /etc/X11/xorg.conf.d/99-calibration.conf
echo "LCD rotate value is set to $1"
elif [ $new_rotate_value -eq 90 ]; then
sudo cp $cur_dir/usr/99-calibration.conf-$device_id-90 /etc/X11/xorg.conf.d/99-calibration.conf
echo "LCD rotate value is set to $1"
elif [ $new_rotate_value -eq 180 ]; then
sudo cp $cur_dir/usr/99-calibration.conf-$device_id-180 /etc/X11/xorg.conf.d/99-calibration.conf
echo "LCD rotate value is set to $1"
elif [ $new_rotate_value -eq 270 ]; then
sudo cp $cur_dir/usr/99-calibration.conf-$device_id-270 /etc/X11/xorg.conf.d/99-calibration.conf
echo "LCD rotate value is set to $1"
elif [ $new_rotate_value -eq 360 ]; then
sudo cp $cur_dir/usr/99-calibration.conf-$device_id-FLIP-H /etc/X11/xorg.conf.d/99-calibration.conf
echo "LCD rotate value is set to flip horizontally"
elif [ $new_rotate_value -eq 450 ]; then
sudo cp $cur_dir/usr/99-calibration.conf-$device_id-FLIP-V /etc/X11/xorg.conf.d/99-calibration.conf
echo "LCD rotate value is set to flip vertically"
fi
elif [ $touch_type = "capacity" ]; then
if [ $new_rotate_value -eq 0 ]; then
sudo cp $cur_dir/usr/40-libinput.conf-0 /etc/X11/xorg.conf.d/40-libinput.conf
echo "LCD rotate value is set to $1"
elif [ $new_rotate_value -eq 90 ]; then
sudo cp $cur_dir/usr/40-libinput.conf-90 /etc/X11/xorg.conf.d/40-libinput.conf
echo "LCD rotate value is set to $1"
elif [ $new_rotate_value -eq 180 ]; then
sudo cp $cur_dir/usr/40-libinput.conf-180 /etc/X11/xorg.conf.d/40-libinput.conf
echo "LCD rotate value is set to $1"
elif [ $new_rotate_value -eq 270 ]; then
sudo cp $cur_dir/usr/40-libinput.conf-270 /etc/X11/xorg.conf.d/40-libinput.conf
echo "LCD rotate value is set to $1"
elif [ $new_rotate_value -eq 360 ]; then
sudo cp $cur_dir/usr/40-libinput.conf-FLIP-H /etc/X11/xorg.conf.d/40-libinput.conf
echo "LCD rotate value is set to flip horizontally"
elif [ $new_rotate_value -eq 450 ]; then
sudo cp $cur_dir/usr/40-libinput.conf-FLIP-V /etc/X11/xorg.conf.d/40-libinput.conf
echo "LCD rotate value is set to flip vertically"
fi
else
echo "Invalid touch type"
exit
fi

sudo sync
sudo sync

echo "reboot now"
sleep 1
sudo reboot

