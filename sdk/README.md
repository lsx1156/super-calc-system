# 超采集算系统 - Pico SDK 构建版本

## 项目概述

本目录包含使用 **Pico SDK** + **CMake** + **Ninja** 构建的超采集算系统固件。

### 架构

```
三级分布式架构
├── Raspberry Pi (控制层) - web服务 + 数据处理
├── Pico2 (协处理层) - USB通信 + SPI主机 + 数据聚合
└── Pico (终端层) - ADC采样 + 数字捕获 + SPI从机
```

## 环境要求

### 工具链安装

使用 MSYS2 UCRT64 环境：

```bash
# 安装工具链
pacman -S mingw-w64-ucrt-x86_64-arm-none-eabi-gcc
pacman -S mingw-w64-ucrt-x86_64-ninja
pacman -S mingw-w64-ucrt-x86_64-cmake
```

### 版本要求

| 工具 | 最低版本 | 推荐版本 |
|------|----------|----------|
| arm-none-eabi-gcc | 13.0.0 | 13.4.0 |
| cmake | 3.13.0 | 4.4.0 |
| ninja | 1.10.0 | 1.13.2 |
| Pico SDK | 2.0.0 | 2.5.1 |

## 目录结构

```
sdk/
├── pico-sdk/              # Pico SDK 核心库
│   ├── lib/FreeRTOS-Kernel/   # FreeRTOS-Kernel V10.5.1
│   └── lib/tinyusb/           # TinyUSB 设备栈
├── SB/                    # 完整版（含所有功能）
│   ├── pico/firmware/         # Pico 终端层固件
│   │   ├── src/               # 10个源文件
│   │   ├── include/           # 头文件 + FreeRTOSConfig.h
│   │   ├── CMakeLists.txt     # 构建配置
│   │   └── pico_sdk_import.cmake
│   ├── pico2/firmware/        # Pico2 协处理层固件
│   │   ├── src/               # 12个源文件
│   │   ├── include/           # 头文件 + FreeRTOSConfig.h
│   │   ├── CMakeLists.txt     # 构建配置
│   │   └── pico_sdk_import.cmake
│   ├── raspberry_pi/          # 树莓派服务端
│   └── tools/                 # 工具脚本
└── demo/                  # Demo版（基础功能）
    ├── pico/firmware/         # Pico 演示固件
    ├── pico2/firmware/        # Pico2 演示固件
    └── raspberry_pi/          # 演示版树莓派服务端
```

## 构建步骤

### 构建 Pico 固件（完整版）

```bash
cd sdk/SB/pico/firmware
mkdir -p build && cd build
cmake -G Ninja -DPICO_TOOLCHAIN_PATH=C:/msys64/ucrt64 ..
ninja
```

### 构建 Pico2 固件（完整版）

```bash
cd sdk/SB/pico2/firmware
mkdir -p build && cd build
cmake -G Ninja -DPICO_TOOLCHAIN_PATH=C:/msys64/ucrt64 ..
ninja
```

### 构建 Demo 版本

```bash
# Pico Demo
cd sdk/demo/pico/firmware
mkdir -p build && cd build
cmake -G Ninja -DPICO_TOOLCHAIN_PATH=C:/msys64/ucrt64 ..
ninja

# Pico2 Demo
cd sdk/demo/pico2/firmware
mkdir -p build && cd build
cmake -G Ninja -DPICO_TOOLCHAIN_PATH=C:/msys64/ucrt64 ..
ninja
```

## 功能模块

### SB 完整版功能

| 模块 | Pico | Pico2 | 说明 |
|------|------|-------|------|
| ADC采样 | ✅ 4路 | - | 12位ADC，最高500ksps |
| 数字捕获 | ✅ 8路 | - | PIO DMA连续捕获 |
| SPI通信 | ✅ 从机 | ✅ 主机 | 高速SPI帧传输 |
| USB CDC | - | ✅ | TinyUSB CDC设备 |
| FreeRTOS | ✅ | ✅ | 实时任务调度 |
| I2C调度 | ✅ 从机 | ✅ 主机 | 自动地址分配 |
| 破解引擎 | ✅ MD5 | - | 暴力破解加速 |
| 故障保护 | - | ✅ | Foolproof模块 |
| 超频控制 | ✅ | ✅ | 动态频率调整 |
| 数据聚合 | - | ✅ | 多节点数据聚合 |

### Demo 版功能

| 模块 | Pico | Pico2 | 说明 |
|------|------|-------|------|
| ADC采样 | ✅ | - | 基础采样 |
| 数字捕获 | ✅ | - | 基础捕获 |
| SPI通信 | ✅ | ✅ | 基础通信 |
| USB CDC | - | ✅ | 基础通信 |
| 双核支持 | ✅ | ✅ | multicore |

## 刷写固件

将 Pico/Pico2 进入 BOOTSEL 模式，复制 `.uf2` 文件到设备：

```
# Pico
copy sdk/SB/pico/firmware/build/super_calc_pico.uf2 E:\

# Pico2
copy sdk/SB/pico2/firmware/build/super_calc_pico2.uf2 E:\
```

## 通信协议

### SPI 帧格式

```
[FRAME_HEADER_PICO][LEN_L][LEN_H][CMD][PARAMS...][CRC_L][CRC_H][FRAME_TAIL_PICO]
```

### USB CDC 帧格式

```
[FRAME_HEADER_USB][VER][LEN_L][LEN_H][CMD][PARAMS...][CRC32][FRAME_TAIL_USB]
```

## 配置参数

### Pico FreeRTOS 配置

| 参数 | 值 | 说明 |
|------|-----|------|
| configCPU_CLOCK_HZ | 125MHz | RP2040默认频率 |
| configTOTAL_HEAP_SIZE | 64KB | 堆大小 |
| configMAX_PRIORITIES | 8 | 优先级数量 |

### Pico2 FreeRTOS 配置

| 参数 | 值 | 说明 |
|------|-----|------|
| configCPU_CLOCK_HZ | 133MHz | RP2350默认频率 |
| configTOTAL_HEAP_SIZE | 128KB | 堆大小 |
| configMAX_PRIORITIES | 8 | 优先级数量 |

## 任务划分

### Pico 任务

| 任务 | 优先级 | 栈大小 | 说明 |
|------|--------|--------|------|
| SPI通信 | 4 | 1024 | SPI从机接收 |
| SPI命令分发 | 4 | 1024 | 命令处理 |
| I2C从机 | 3 | 1024 | I2C调度 |
| 采样 | 2 | 512 | ADC/Digital采样 |
| 破解 | 2 | 2048 | 暴力破解 |
| 监控 | 1 | 512 | 温度/状态监控 |

### Pico2 任务

| 任务 | 优先级 | 栈大小 | 说明 |
|------|--------|--------|------|
| Foolproof | 5 | 256 | 故障保护 |
| DMA采集 | 4 | 256 | SPI DMA接收 |
| 数据聚合 | 4 | 1024 | 多节点聚合 |
| USB通信 | 3 | 1024 | CDC通信 |
| I2C调度 | 3 | 256 | I2C主机 |
| 温度监控 | 2 | 256 | 温度监测 |
| 命令分发 | 3 | 512 | USB命令处理 |

## 故障排查

### 常见问题

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 编译器未找到 | arm-none-eabi-gcc路径错误 | 设置 PICO_TOOLCHAIN_PATH |
| FreeRTOS.h 未找到 | FreeRTOS未导入 | 检查 FreeRTOS_Kernel_import.cmake |
| 链接错误 | 缺少库 | 检查 target_link_libraries |
| tinyusb 冲突 | pico_stdio_usb与tinyusb_device冲突 | 使用其中一个 |

### 调试建议

```bash
# 启用调试信息
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..

# 使用 picotool 查看设备信息
picotool info
```

## 版本信息

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0.0 | 2026-07-12 | 初始版本 |

## 许可证

MIT License