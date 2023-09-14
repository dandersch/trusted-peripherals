#!/bin/bash

set -e # exit on error

# source this file inside your zephyrfolder to get a reference to the Zephyr SDK and toolchain
source ../zephyrproject/zephyr/zephyr-env.sh

# SET TRUSTZONE OPTION BYTES BEFORE FLASHING IF NEVER DONE BEFORE
#./build/tfm/regression.sh

# flash the application onto the board
west flash --runner=openocd --build-dir build

# To connect to serial port to see output, use
#minicom -D /dev/ttyACM0 --color=on
