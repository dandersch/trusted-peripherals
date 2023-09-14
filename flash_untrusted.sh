#!/bin/bash

set -e # exit on error

# source this file inside your zephyrfolder to get a reference to the Zephyr SDK and toolchain
source ../zephyrproject/zephyr/zephyr-env.sh

# flash onto the board with openocd (pyocd fails)
west flash --runner=openocd --build-dir build_untrusted

# To connect to a serial port to see output, use
# minicom -D /dev/ttyACM0 --color=on
