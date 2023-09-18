#!/bin/bash

set -e # exit on error

# source this file inside your zephyrfolder to get a reference to the Zephyr SDK and toolchain
source ../zephyrproject/zephyr/zephyr-env.sh

west build . -p -b mps2_an521_ns --build-dir build_emulated -t run -- -DEMULATED=1

# emulated untrusted setup
#west build . -p -b mps2_an521 --build-dir build_emulated -t run -- -DEMULATED=1 -DUNTRUSTED=1
