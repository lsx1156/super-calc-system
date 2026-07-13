# 超采集算系统 - Pico2协处理层固件（完整版）
# 自适应集群扩展 + 多模式支持 + FreeRTOS 5任务
cmake_minimum_required(VERSION 3.13)

set(PICO_PLATFORM rp2350)

include(pico_sdk_import.cmake)

project(super_calc_pico2 C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

# FreeRTOS-Kernel integration
set(FREERTOS_KERNEL_PATH ${PICO_SDK_PATH}/lib/FreeRTOS-Kernel CACHE PATH "Path to FreeRTOS Kernel")
set(FREERTOS_CONFIG_FILE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include CACHE STRING "FreeRTOSConfig.h directory")
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

set(SOURCES
    src/main.c
    src/system_status.c
    src/cluster_mgr.c
    src/spi_master.c
    src/data_aggregator.c
    src/usb_cdc_comm.c
    src/foolproof.c
    src/oc_control.c
    src/addr_assigner.c
    src/edge_compute.c
    src/i2c_sched_master.c
    src/dma_spi_harvester.c
)

set(INCLUDES
    include
)

add_executable(super_calc_pico2 ${SOURCES})

target_include_directories(super_calc_pico2 PRIVATE ${INCLUDES})

target_link_libraries(super_calc_pico2
    pico_stdlib
    pico_multicore
    hardware_spi
    hardware_gpio
    hardware_adc
    hardware_dma
    hardware_clocks
    hardware_vreg
    hardware_watchdog
    hardware_timer
    tinyusb_device
    tinyusb_board
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
)

pico_add_extra_outputs(super_calc_pico2)

target_compile_definitions(super_calc_pico2 PRIVATE
    FULL_VERSION=1
    PICO_STACK_SIZE=4096
)

target_compile_options(super_calc_pico2 PRIVATE -O2)
