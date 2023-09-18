add_library(tfm_app_rot_partition_tp STATIC)

target_compile_definitions(tfm_app_rot_partition_tp PUBLIC TRUSTED)

target_include_directories(tfm_app_rot_partition_tp
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    PRIVATE
        ${CMAKE_BINARY_DIR}/trusted_peripheral
        ../driver/oled_driver
        ../driver/oled_driver/Config
        ../driver/oled_driver/OLED
        ../driver/oled_driver/GUI
        ../driver/oled_driver/Example
)
target_include_directories(tfm_partitions
    INTERFACE
        ${CMAKE_BINARY_DIR}/trusted_peripheral
)

if(DEFINED EMULATED)
    message("#################################################")
    message(STATUS "Building emulated setup")
    message("#################################################")
    target_compile_definitions(tfm_app_rot_partition_tp PUBLIC EMULATED)
else()
    file(GLOB_RECURSE oled_driver_sources CONFIGURE_DEPENDS ../driver/oled_driver/*.c)
endif()

# included because driver code is full of these warnings
target_compile_options(tfm_app_rot_partition_tp PRIVATE -Wno-missing-braces)
target_compile_options(tfm_app_rot_partition_tp PRIVATE -Wno-comment)

target_sources(tfm_app_rot_partition_tp
    PRIVATE
        trusted_peripheral.c
        ${oled_driver_sources}
)

# The generated sources
target_sources(tfm_app_rot_partition_tp
    PRIVATE
        ${CMAKE_BINARY_DIR}/trusted_peripheral/auto_generated/intermedia_tfm_trusted_peripheral.c
)
target_sources(tfm_partitions
    INTERFACE
        ${CMAKE_BINARY_DIR}/trusted_peripheral/auto_generated/load_info_tfm_trusted_peripheral.c
)

target_link_libraries(tfm_app_rot_partition_tp
    PRIVATE
        platform_s
        tfm_sprt
)

target_link_libraries(tfm_spm
    PRIVATE
        tfm_app_rot_partition_tp
)

############################ Partition Defs ####################################

target_link_libraries(tfm_partitions
    INTERFACE
        tfm_app_rot_partition_tp
)

target_compile_definitions(tfm_partition_defs
    INTERFACE
        TFM_PARTITION_TRUSTED_PERIPHERAL
)
