# 超采集算系统 Demo - Pico2协处理器固件
# 单组验证版：1片RP2350 + 8片RP2040
cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(super_calc_demo_pico2 C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

# 源文件
add_executable(super_calc_demo_pico2
    main.c
)

# 链接库
target_link_libraries(super_calc_demo_pico2
    pico_stdlib
    pico_multicore
    hardware_spi
    hardware_gpio
    hardware_dma
    hardware_clocks
    hardware_vreg
    hardware_adc
    tinyusb_device
    tinyusb_board
)

# 生成输出文件
pico_add_extra_outputs(super_calc_demo_pico2)

# Demo编译选项
target_compile_definitions(super_calc_demo_pico2 PRIVATE
    DEMO_VERSION=1
    NUM_PICO_SLAVES=8
)