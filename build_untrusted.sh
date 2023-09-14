#!/bin/bash

set -e # exit on error

# source this file inside your zephyrfolder to get a reference to the Zephyr SDK and toolchain
source ../zephyrproject/zephyr/zephyr-env.sh

# compile this application
west build -p always -b nucleo_l552ze_q . --build-dir build_untrusted -- -DUNTRUSTED=1

# To connect to a serial port to see output, use 
# minicom -D /dev/ttyACM0 --color=on
