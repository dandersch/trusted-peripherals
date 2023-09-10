cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(i2c_sensor)

target_include_directories(app PRIVATE src/oled_driver )
target_include_directories(app PRIVATE src/oled_driver/Config )
target_include_directories(app PRIVATE src/oled_driver/OLED )
target_include_directories(app PRIVATE src/oled_driver/GUI )
target_include_directories(app PRIVATE src/oled_driver/Example )
file(GLOB_RECURSE oled_driver_sources CONFIGURE_DEPENDS src/oled_driver/*.c)

target_sources(app PRIVATE
        ${oled_driver_sources}
        src/main.c
        )
