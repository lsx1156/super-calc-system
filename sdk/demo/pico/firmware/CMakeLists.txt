# 超采集算系统 Demo - Pico终端芯片固件
# 单组验证版：8片RP2040
cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(super_calc_demo_pico C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

# 源文件
add_executable(super_calc_demo_pico
    main.c
)

# 链接库
target_link_libraries(super_calc_demo_pico
    pico_stdlib
    pico_multicore
    hardware_adc
    hardware_dma
    hardware_spi
    hardware_gpio
    hardware_irq
    hardware_clocks
    hardware_vreg
    hardware_pio
)

# 生成输出文件
pico_add_extra_outputs(super_calc_demo_pico)

# Demo编译选项
target_compile_definitions(super_calc_demo_pico PRIVATE
    DEMO_VERSION=1
    PICO_DEFAULT_FREQ_KHZ=133000
)