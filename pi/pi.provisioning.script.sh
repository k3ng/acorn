#!/bin/bash

# TODO: more work is needed to be done on this

# This script prepares the Pi, downloading and installing everything needed

echo -e "\r\nAcorn Raspberry Pi Provisioning Script Version 20230313\r\n"

if [ "$EUID" -ne 0 ]
  then echo "Please run with root privileges (sudo).  Exiting..."
  exit
fi

apt install arduino avrdude and avrdude-docs arduino-mk libasound2-dev libfftw3-dev crony wiringpi libpulse-dev libsox-fmt-pulse libpulsedsp lxplug-volumepulse pulse-module-gsettings

cat << EOF >> /usr/share/arduino/hardware/tools/avrdude.conf
programmer
  id    = "linuxgpio";
  desc  = "Use the Linux sysfs interface to bitbang GPIO lines";
  type  = "linuxgpio";
  reset = 4;
  sck   = 11;
  mosi  = 10;
  miso  = 9;
;
EOF

cp -r ~/acorn/avr-hardware/mainunit/required.libraries/* /usr/share/arduino/libraries/

# Disable serial console on GPIO serial in /boot/cmdline.txt. Find the following text and remove it:

# console=serial0,115200

# In /boot/config.txt make these changes:

# #dtparam=audio=on
# dtparam=i2c_arm=on
# dtparam=i2s=on
# enable_uart=1
# #dtoverlay=audioinjector-wm8731-audio
# dtoverlay=i2s-mmap

echo -e "Rebooting...\r\n"

sleep 5

reboot

