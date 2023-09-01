#!/bin/bash
#
# "Tests for verifying implementations of TBSA-v8M and the PSA Certified APIs"
# NOTE: none of the tests are supported by our target board STM32L552ZE_Q
# Supported targets are:
#       tgt_dev_apis_linux
#       tgt_dev_apis_stdc
#       tgt_dev_apis_tfm_an521
#       tgt_dev_apis_tfm_an524
#       tgt_dev_apis_tfm_an539
#       tgt_dev_apis_tfm_cs3x0
#       tgt_dev_apis_tfm_musca_a
#       tgt_dev_apis_tfm_musca_b1
#       tgt_dev_apis_tfm_musca_s1
#       tgt_dev_apis_tfm_nrf5340
#       tgt_dev_apis_tfm_nrf9160
#       tgt_dev_apis_tfm_psoc64
#       tgt_dev_apis_tfm_stm32l562e_dk
#       tgt_ff_tfm_an521
#       tgt_ff_tfm_cs3x0
#       tgt_ff_tfm_musca_a
#       tgt_ff_tfm_musca_b1
#       tgt_ff_tfm_musca_s1
#       tgt_ff_tfm_nrf5340
#       tgt_ff_tfm_nrf9160
#       tgt_ff_tfm_nrf_common

set -e # exit on error

# source this file inside your zephyrfolder to get a reference to the Zephyr SDK and toolchain
source ../../zephyrproject/zephyr/zephyr-env.sh

#west build -p always -b nucleo_l552ze_q_ns . -- -DCONFIG_TFM_PSA_TEST_CRYPTO=y
#./build/tfm/regression.sh
#west flash --runner=openocd
#west build -p always -b nucleo_l552ze_q_ns . -- -DCONFIG_TFM_PSA_TEST_PROTECTED_STORAGE=y
#./build/tfm/regression.sh
#west flash --runner=openocd
#west build -p always -b nucleo_l552ze_q_ns . -- -DCONFIG_TFM_PSA_TEST_INTERNAL_TRUSTED_STORAGE=y
#./build/tfm/regression.sh
#west flash --runner=openocd
#west build -p always -b nucleo_l552ze_q_ns . -- -DCONFIG_TFM_PSA_TEST_STORAGE=y
#./build/tfm/regression.sh
#west flash --runner=openocd
#west build -p always -b nucleo_l552ze_q_ns . -- -DCONFIG_TFM_PSA_TEST_INITIAL_ATTESTATION=y
#./build/tfm/regression.sh
#west flash --runner=openocd

# run with qemu
west build . -p -b mps2_an521_ns -t run -- -DCONFIG_TFM_PSA_TEST_CRYPTO=y
west build . -p -b mps2_an521_ns -t run -- -DCONFIG_TFM_PSA_TEST_PROTECTED_STORAGE=y
west build . -p -b mps2_an521_ns -t run -- -DCONFIG_TFM_PSA_TEST_INTERNAL_TRUSTED_STORAGE=y
west build . -p -b mps2_an521_ns -t run -- -DCONFIG_TFM_PSA_TEST_STORAGE=y
west build . -p -b mps2_an521_ns -t run -- -DCONFIG_TFM_PSA_TEST_INITIAL_ATTESTATION=y
