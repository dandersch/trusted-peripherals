add_library(tfm_app_rot_partition_tp STATIC)

target_include_directories(tfm_app_rot_partition_tp
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    PRIVATE
        ${CMAKE_BINARY_DIR}/trusted_peripheral
)
target_include_directories(tfm_partitions
    INTERFACE
        ${CMAKE_BINARY_DIR}/trusted_peripheral
)

target_sources(tfm_app_rot_partition_tp
    PRIVATE
        trusted_peripheral.c
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
