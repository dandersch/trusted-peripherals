cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(i2c_sensor)

if(DEFINED UNTRUSTED)
    message("#################################################")
    message("#################################################")
    message(STATUS "Building untrusted setup")
    message("#################################################")
    message("#################################################")
    target_compile_definitions(app PUBLIC UNTRUSTED)

    # OLED DRIVER
    if(DEFINED EMULATED)
        target_compile_definitions(app PUBLIC EMULATED)
    else()
        target_include_directories(app PRIVATE driver/oled_driver )
        target_include_directories(app PRIVATE driver/oled_driver/Config )
        target_include_directories(app PRIVATE driver/oled_driver/OLED )
        target_include_directories(app PRIVATE driver/oled_driver/GUI )
        target_include_directories(app PRIVATE driver/oled_driver/Example )
        file(GLOB_RECURSE oled_driver_sources CONFIGURE_DEPENDS driver/oled_driver/*.c)
    endif()

    target_include_directories(app PRIVATE src/ )

    # included because driver code is full of these warnings
    target_compile_options(app PRIVATE -Wno-missing-braces)
    target_compile_options(app PRIVATE -Wno-comment)

    target_sources(app PRIVATE
            ${oled_driver_sources}
            src/main.c
            trusted_peripheral/trusted_peripheral.c
    )
else()
    message("#################################################")
    message("#################################################")
    message(STATUS "Building trusted setup")
    message("#################################################")
    message("#################################################")
    target_compile_definitions(app PUBLIC TRUSTED)

    # NOTE force test flash layout for more flash memory
    set_property(TARGET zephyr_property_target
                 APPEND PROPERTY TFM_CMAKE_OPTIONS
                 -DTFM_S_REG_TEST=ON
                 #-DCRYPTO_HW_ACCELERATOR=OFF # doesn't work
    )

    target_sources(app PRIVATE
            src/main.c
            src/trusted_peripheral_ns.c
    )

    # secure partition
    get_target_property(TFM_BINARY_DIR tfm TFM_BINARY_DIR)
    configure_file(
      ${CMAKE_CURRENT_LIST_DIR}/trusted_peripheral/tfm_manifest_list.yaml.in
      ${CMAKE_CURRENT_BINARY_DIR}/trusted_peripheral/tfm_manifest_list.yaml
    )
    
    set_property(TARGET zephyr_property_target
      APPEND PROPERTY TFM_CMAKE_OPTIONS
      -DTFM_EXTRA_MANIFEST_LIST_FILES=${CMAKE_CURRENT_BINARY_DIR}/trusted_peripheral/tfm_manifest_list.yaml
      -DTFM_EXTRA_PARTITION_PATHS=${CMAKE_CURRENT_LIST_DIR}/trusted_peripheral
    )
    
if(DEFINED EMULATED)
    target_compile_definitions(app PUBLIC EMULATED)
    set_property(TARGET zephyr_property_target
      APPEND PROPERTY TFM_CMAKE_OPTIONS
      -DEMULATED=1
    )
else()
endif()
    
    # include folder that has tfm_api.h
    target_include_directories(app PRIVATE
      $<TARGET_PROPERTY:tfm,TFM_BINARY_DIR>/install/interface/include
    )
    
    target_compile_definitions(app
        PRIVATE TFM_PARTITION_TRUSTED_PERIPHERAL
    )
endif()
