#!/bin/bash

# source this file inside your zephyrfolder to get a reference to the Zephyr SDK and toolchain
source ../../zephyrproject/zephyr/zephyr-env.sh

# compile this application
west build -p always -b nucleo_l552ze_q .

# flash onto the board with openocd (pyocd fails)
west flash --runner=openocd

# connect to serial port to see output
minicom -D /dev/ttyACM0 --color=on
