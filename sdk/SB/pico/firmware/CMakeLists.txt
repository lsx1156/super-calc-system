# 超采集算系统 - Pico终端层固件（完整版）
# 支持自适应集群、多模式切换
cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(super_calc_pico C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

# FreeRTOS-Kernel integration
set(FREERTOS_KERNEL_PATH ${PICO_SDK_PATH}/lib/FreeRTOS-Kernel CACHE PATH "Path to FreeRTOS Kernel")
set(FREERTOS_CONFIG_FILE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include CACHE STRING "FreeRTOSConfig.h directory")
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

# 源文件列表
set(SOURCES
    src/main.c
    src/status_mgr.c
    src/adc_sample.c
    src/digital_capture.c
    src/spi_comm.c
    src/overclock.c
    src/crack_engine.c
    src/hw_test.c
    src/i2c_sched_slave.c
    src/i2c_sched_regs.c
)

# 头文件路径
set(INCLUDES
    include
)

# 可执行文件
add_executable(super_calc_pico ${SOURCES})

# PIO 头文件生成
pico_generate_pio_header(super_calc_pico ${CMAKE_CURRENT_LIST_DIR}/src/digital_capture.pio)

target_include_directories(super_calc_pico PRIVATE ${INCLUDES})

# 链接库
target_link_libraries(super_calc_pico
    pico_stdlib
    pico_multicore
    pico_i2c_slave
    hardware_spi
    hardware_gpio
    hardware_adc
    hardware_dma
    hardware_clocks
    hardware_vreg
    hardware_watchdog
    hardware_timer
    hardware_pio
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
)

# 生成输出文件
pico_add_extra_outputs(super_calc_pico)

# 编译选项
target_compile_definitions(super_calc_pico PRIVATE
    FULL_VERSION=1
    PICO_STACK_SIZE=2048
)

# 优化级别
target_compile_options(super_calc_pico PRIVATE -O2)
