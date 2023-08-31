#!/bin/bash

# following change was necessary in:
# /usr/lib/python3.11/site-packages/mbed_tools/devices/_internal/linux/device_detector.py
# if disk.properties.get("ID_VENDOR_ID") != "0483": # STMelectronics vendor id
#     continue

# make sure the board (should be /dev/sdd) is mounted to a folder with user perms, i.e.
# sudo mount_as_user.sh /dev/sdd /mnt/nas1
findmnt --source "/dev/sdd" >/dev/null || exit

# To compile a program for this board using Mbed CLI, use NUCLEO_L552ZE_Q as the target name.
mbed-tools compile -m NUCLEO_L552ZE_Q -t GCC_ARM --flash --sterm --profile debug
