#!/bin/bash

set -e # exit on error

# source this file inside your zephyrfolder to get a reference to the Zephyr SDK and toolchain
source ../../zephyrproject/zephyr/zephyr-env.sh

# compile this application
west build -p always -b nucleo_l552ze_q_ns .

# set TrustZone option bytes on the board
./build/tfm/regression.sh

# flash the application onto the board
west flash --runner=openocd

# connect to serial port to see output
#minicom -D /dev/ttyACM0 --color=on

# QEMU
#west build . -p -b mps2_an521_ns # build for board that is supported by QEMU
#qemu-system-arm -M mps2-an521 -device loader,file=build/zephyr/tfm_merged.hex -serial stdio # run in qemu
