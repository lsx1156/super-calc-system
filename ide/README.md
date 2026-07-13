# 超采集算系统 - Arduino IDE 构建版本

## 项目概述

本目录包含使用 **Arduino IDE** + **Raspberry Pi Pico 插件** 构建的超采集算系统固件。

## 环境要求

### Arduino IDE 安装

1. 下载并安装 [Arduino IDE 2.x](https://www.arduino.cc/en/software)
2. 安装 Raspberry Pi Pico 插件：
   - 打开 Arduino IDE → 工具 → 开发板 → 开发板管理器
   - 搜索 "Raspberry Pi Pico" 并安装

### 开发板包

| 开发板 | 包名称 | 版本 |
|--------|--------|------|
| Raspberry Pi Pico | rp2040:rp2040 | >=3.6.0 |
| Raspberry Pi Pico2 | rp2040:rp2350 | >=3.6.0 |

### 配置开发板

```
工具 → 开发板 → Raspberry Pi Pico Boards → Raspberry Pi Pico
工具 → 端口 → 选择正确的串口
工具 → 上传方式 → UF2
```

## 目录结构

```
ide/
└── arduino/
    ├── pico_sb/              # Pico 完整版
    │   ├── pico_sb.ino       # 入口文件
    │   └── src/              # 9个源文件 + 头文件
    │       ├── adc_sample.cpp/h
    │       ├── crack_engine.cpp/h
    │       ├── digital_capture.cpp/h
    │       ├── hw_test.cpp/h
    │       ├── i2c_sched_slave.cpp/h
    │       ├── overclock.cpp/h
    │       ├── spi_comm.cpp/h
    │       └── status_mgr.cpp/h
    ├── pico2_sb/             # Pico2 完整版
    │   ├── pico2_sb.ino      # 入口文件
    │   └── src/              # 6个源文件 + 头文件
    │       ├── cluster_mgr.cpp/h
    │       ├── oc_control.cpp/h
    │       ├── spi_master.cpp/h
    │       ├── system_status.cpp/h
    │       └── usb_cdc_comm.cpp/h
    ├── pico_demo/            # Pico 演示版
    │   └── pico_demo.ino     # 单文件
    └── pico2_demo/           # Pico2 演示版
        └── pico2_demo.ino    # 单文件
```

## 编译与上传

### 编译 Pico 完整版

1. 打开 `ide/arduino/pico_sb/pico_sb.ino`
2. 选择开发板：`工具 → 开发板 → Raspberry Pi Pico`
3. 点击 "验证" 按钮编译
4. 点击 "上传" 按钮刷写固件

### 编译 Pico2 完整版

1. 打开 `ide/arduino/pico2_sb/pico2_sb.ino`
2. 选择开发板：`工具 → 开发板 → Raspberry Pi Pico2`
3. 点击 "验证" 按钮编译
4. 点击 "上传" 按钮刷写固件

### 编译 Demo 版本

1. 打开对应 `.ino` 文件
2. 选择开发板
3. 编译并上传

## 功能模块

### SB 完整版功能

| 模块 | Pico | Pico2 | 说明 |
|------|------|-------|------|
| ADC采样 | ✅ 4路 | - | 12位ADC采样 |
| 数字捕获 | ✅ 8路 | - | 数字信号捕获 |
| SPI通信 | ✅ 从机 | ✅ 主机 | 高速SPI |
| USB通信 | - | ✅ Serial | CDC串口 |
| I2C调度 | ✅ 从机 | ✅ 主机 | 地址分配 |
| 破解引擎 | ✅ MD5 | - | 暴力破解 |
| 超频控制 | ✅ | ✅ | CPU超频 |
| 状态管理 | ✅ | ✅ | 设备状态 |
| 集群管理 | - | ✅ | 多节点管理 |

### Demo 版功能

| 模块 | Pico | Pico2 | 说明 |
|------|------|-------|------|
| ADC采样 | ✅ | - | 基础采样 |
| 数字捕获 | ✅ | - | 基础捕获 |
| SPI通信 | ✅ | ✅ | 基础通信 |
| USB通信 | - | ✅ | 基础通信 |
| 超频控制 | ✅ | ✅ | 基础超频 |

## 配置参数

### config.h 配置

```cpp
// Pico 配置
#define DEFAULT_FREQ_KHZ 125000UL  // 默认频率 125MHz
#define WATCHDOG_TIMEOUT 5000      // 看门狗超时 5s
#define NODE_ID 0x00               // 默认节点ID

// Pico2 配置
#define MAX_PICO_SLAVES 8          // 最大从机数量
#define USB_RX_BUFFER_SIZE 512     // USB接收缓冲区
#define USB_TX_BUFFER_SIZE 1024    // USB发送缓冲区
```

## 通信协议

### SPI 帧格式

```
[FRAME_HEADER_PICO][LEN_L][LEN_H][CMD][PARAMS...][CRC_L][CRC_H][FRAME_TAIL_PICO]
```

### USB Serial 帧格式

```
[FRAME_HEADER_USB][VER][LEN_L][LEN_H][CMD][PARAMS...][CRC32][FRAME_TAIL_USB]
```

## 任务结构（非FreeRTOS）

Arduino 版本使用 `loop()` 轮询模式，包含以下子循环：

### Pico 轮询循环

| 循环 | 说明 |
|------|------|
| SPI接收 | 处理SPI从机数据 |
| 采样 | ADC/Digital采样 |
| 状态更新 | 设备状态管理 |
| I2C从机 | I2C调度通信 |

### Pico2 轮询循环

| 循环 | 说明 |
|------|------|
| USB接收 | 处理USB命令 |
| SPI主机 | 向Pico发送命令 |
| 数据聚合 | 聚合多节点数据 |
| 状态更新 | 系统状态管理 |

## 故障排查

### 常见问题

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 编译错误 | 缺少库 | 检查 src/ 目录完整 |
| 上传失败 | 开发板未进入BOOTSEL | 按住BOOTSEL再上传 |
| 串口无响应 | 端口选择错误 | 检查设备管理器 |
| SPI通信失败 | 引脚配置错误 | 检查 config.h |

### 调试建议

```cpp
// 在代码中添加调试输出
Serial.begin(115200);
Serial.println("System initialized");
```

## 版本信息

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0.0 | 2026-07-12 | 初始版本 |

## 许可证

MIT License