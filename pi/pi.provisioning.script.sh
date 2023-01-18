#!/bin/bash

# This script prepares the Pi, downloading and installing everything needed

echo -e "\r\nAcorn Raspberry Pi Provisioning Script Version 20230117\r\n"

if [ "$EUID" -ne 0 ]
  then echo "Please run with root privileges (sudo).  Exiting..."
  exit
fi

apt-get install arduino avrdude avrdude-docs arduino-mk github-cli

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

cp -r ~/acorn/acornarduino/required.libraries/* /usr/share/arduino/libraries/



