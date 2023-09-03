cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(i2c_sensor)

#target_include_directories(app PRIVATE
#        stm32l5xx_hal_driver/Inc
#        )
#file(GLOB stm_hal_sources CONFIGURE_DEPENDS stm32l5xx_hal_driver/Src/*.c)

target_sources(app PRIVATE
        #${stm_hal_sources}
        src/main.c
        )
