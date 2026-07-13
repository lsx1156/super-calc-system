# 超采集算系统

Super Collection Compute System

[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

## 概述

超采集算系统是一套三层分布式架构的高性能信号采集与计算平台，支持自适应集群扩展和多种工作模式。系统采用树莓派作为主控层，Pico2 (RP2350) 作为协处理层，Pico (RP2040) 作为终端采集层，实现大规模并行数据采集与边缘计算。

## 系统架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                      树莓派 4B/5 主控层                              │
│  ┌──────────┬──────────┬──────────┬──────────────────────────────┐  │
│  │ Web服务   │ 模式调度  │ 集群管理  │ 8大守护进程                   │  │
│  │           │          │          │                              │  │
│  │           │          │          │ 算法引擎(向量化+并行)          │  │
│  └──────────┴──────────┴──────────┴──────────────────────────────┘  │
└───────────────────────────┬────────────────────────────────────────┘
                            │ USB CDC (100Mbps) × 8
┌───────────────────────────┴────────────────────────────────────────┐
│              Pico2 (RP2350) 协处理层 × 8                           │
│  ┌──────────┬──────────┬──────────┬──────────────────────────────┐  │
│  │数据聚合   │ SPI主控   │ 超频控制   │ 状态管理                    │  │
│  └──────────┴──────────┴──────────┴──────────────────────────────┘  │
└───────────────────────────┬────────────────────────────────────────┘
                            │ SPI (20Mbps) × 128
┌───────────────────────────┴────────────────────────────────────────┐
│               Pico (RP2040) 终端层 × 128                           │
│  ┌──────────┬──────────┬──────────┬──────────────────────────────┐  │
│  │ADC采样    │数字捕获   │破解引擎   │ 硬件测试                     │  │
│  └──────────┴──────────┴──────────┴──────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

## 核心特性

- **自适应集群扩展**：动态节点检测、故障自动恢复、负载均衡调度、热插拔支持
- **多模式调度系统**：采样模式、破译模式、暴力破解、硬件测试
- **高性能算法引擎**：向量化优化（20-258x加速）、并行处理框架、流式处理
- **智能自动识别**：模拟信号识别、协议自动识别、哈希类型识别
- **DMA SPI收割架构**：16个DMA通道并行收割、边缘计算、Delta压缩、数据精简80%
- **I2C专用调度总线**：硬件确定性同步，同步精度<1μs
- **SPI片选线自动地址分配**：物理槽位=逻辑地址，统一固件即插即用

## 目录结构

```
rp/
├── LICENSE               # 许可证文件
├── .gitignore            # Git忽略规则
├── README.md             # 项目总览（本文档）
├── sdk/                  # Pico SDK 版本工程
│   ├── SB/               # 完整版
│   │   ├── raspberry_pi/ # 树莓派主控层（Python）
│   │   ├── pico/         # Pico终端层固件（C）
│   │   ├── pico2/        # Pico2协处理层固件（C）
│   │   └── README.md
│   └── demo/             # Demo版
├── ide/                  # Arduino IDE 版本工程
│   └── arduino/
│       ├── pico_sb/      # Pico完整版固件
│       ├── pico2_sb/     # Pico2完整版固件
│       ├── pico_demo/    # Pico Demo版
│       ├── pico2_demo/   # Pico2 Demo版
│       └── README.md
└── hardware/             # 硬件设计文档（可选）
```

## 快速开始

### 环境要求

- 树莓派 4B/5 (或其他Linux主机)
- Python 3.8+
- Pico SDK 2.x（编译固件用）
- Arduino IDE 2.0+（Arduino版本）

### 安装依赖

```bash
cd sdk/SB/raspberry_pi
pip install -r requirements.txt
```

### 启动系统

```bash
cd sdk/SB/raspberry_pi
python main.py --mode watchdog
```

### 访问Web界面

启动后访问：`http://<树莓派IP>:5000`

## 硬件规格

### 模拟采集
- 分辨率：12位
- 采样率：最高125KSPS（单通道）
- 通道数：512路（128节点 × 4通道/节点）

### 数字捕获
- 通道数：1024路（128节点 × 8通道/节点）
- 最高采样率：100MSPS

### 通信接口
- USB CDC：100Mbps（树莓派 ↔ Pico2）
- SPI：20Mbps（Pico2 ↔ Pico）
- I2C：400kHz（控制平面）

## 硬件连接表

### Pico (RP2040) 终端层引脚分配

| 功能 | GPIO | 方向 | 说明 |
|------|------|------|------|
| SPI SCK | GP2 | IN | 数据平面时钟 |
| SPI MOSI | GP3 | IN | 数据平面输入（从Pico2接收） |
| SPI MISO | GP4 | OUT | 数据平面输出（发送到Pico2） |
| SPI CS | GP1 | IN | 片选（由Pico2控制） |
| I2C SDA | GP0 | IN/OUT | 控制平面数据线 |
| I2C SCL | GP5 | IN | 控制平面时钟线 |
| ADC 0 | GP26 | IN | 模拟输入通道0 |
| ADC 1 | GP27 | IN | 模拟输入通道1 |
| ADC 2 | GP28 | IN | 模拟输入通道2 |
| ADC 3 | GP29 | IN | 模拟输入通道3 |
| DIG 0-3 | GP6-GP9 | IN | 数字捕获通道（采样模式） |
| DIG 0-7 | GP0-GP7 | IN | 数字捕获通道（破译模式） |
| PRSNT# | GP22 | IN | PCIe卡检测引脚（低电平有效） |
| RUN_LED | GP25 | OUT | 运行指示灯 |

### Pico2 (RP2350) 协处理层引脚分配

| 功能 | GPIO | 方向 | 说明 |
|------|------|------|------|
| SPI1 SCK | GP10 | OUT | 数据平面时钟 |
| SPI1 MOSI | GP11 | OUT | 数据平面输出（发送到Pico） |
| SPI1 MISO | GP12 | IN | 数据平面输入（从Pico接收） |
| SPI1 CS0-15 | GP2-GP17 | OUT | 片选线（16个Pico节点） |
| I2C0 SDA | GP26 | IN/OUT | 控制平面数据线 |
| I2C0 SCL | GP27 | OUT | 控制平面时钟线 |
| PRSNT# | GP18 | IN | PCIe卡检测引脚（低电平有效） |
| FAULT_LED | GP13 | OUT | 故障红灯（高电平点亮） |
| RUN_LED | GP25 | OUT | 运行指示灯 |

### 树莓派主控层连接

| 功能 | 接口 | 说明 |
|------|------|------|
| USB CDC | USB-A | 连接Pico2（最多8个） |
| I2C | GPIO2/GPIO3 | 可选扩展I2C设备 |
| SPI | GPIO10/GPIO11/GPIO12/GPIO8 | 可选扩展SPI设备 |

### 系统连接示意图

```
树莓派 4B/5
    │
    ├── USB-A 0 ── Pico2 #0
    ├── USB-A 1 ── Pico2 #1
    ├── USB-A 2 ── Pico2 #2
    ├── USB-A 3 ── Pico2 #3
    ├── USB-A 4 ── Pico2 #4
    ├── USB-A 5 ── Pico2 #5
    ├── USB-A 6 ── Pico2 #6
    └── USB-A 7 ── Pico2 #7

Pico2 #0 (RP2350)
    │
    ├── SPI CS0 ── Pico #00 (GP1)
    ├── SPI CS1 ── Pico #01 (GP1)
    ├── SPI CS2 ── Pico #02 (GP1)
    ├── SPI CS3 ── Pico #03 (GP1)
    ├── SPI CS4 ── Pico #04 (GP1)
    ├── SPI CS5 ── Pico #05 (GP1)
    ├── SPI CS6 ── Pico #06 (GP1)
    ├── SPI CS7 ── Pico #07 (GP1)
    ├── SPI CS8 ── Pico #08 (GP1)
    ├── SPI CS9 ── Pico #09 (GP1)
    ├── SPI CS10 ── Pico #10 (GP1)
    ├── SPI CS11 ── Pico #11 (GP1)
    ├── SPI CS12 ── Pico #12 (GP1)
    ├── SPI CS13 ── Pico #13 (GP1)
    ├── SPI CS14 ── Pico #14 (GP1)
    └── SPI CS15 ── Pico #15 (GP1)
```

### 供电要求

| 设备 | 电压 | 典型电流 | 总功率（8台Pico2+128台Pico） |
|------|------|----------|------------------------------|
| Pico (RP2040) | 3.3V | ~30mA | ~4W |
| Pico2 (RP2350) | 3.3V | ~50mA | ~1.3W |
| 树莓派 4B | 5V | ~500mA | ~2.5W |
| **总计** | - | - | **~7.8W** |

### 线缆建议

| 连接 | 线缆类型 | 长度限制 |
|------|----------|----------|
| 树莓派 ↔ Pico2 | USB 3.0 A-to-C | ≤2m |
| Pico2 ↔ Pico (SPI) | 扁平排线 | ≤15cm |
| Pico2 ↔ Pico (I2C) | 扁平排线 | ≤15cm |
| 模拟输入 | 屏蔽线 | ≤50cm |

## 编译固件

### Pico SDK 版本

```bash
# Pico 终端层
cd sdk/SB/pico/firmware
mkdir build && cd build
cmake -G Ninja .. -DPICO_SDK_PATH=/path/to/pico-sdk
ninja

# Pico2 协处理层
cd sdk/SB/pico2/firmware
mkdir build && cd build
cmake -G Ninja .. -DPICO_SDK_PATH=/path/to/pico-sdk
ninja
```

### Arduino 版本

1. 打开 Arduino IDE
2. 安装 Raspberry Pi Pico/RP2040 和 Pico 2/RP2350 板包
3. 打开对应的 `.ino` 文件
4. 选择正确的开发板并上传

## 协议一致性

系统采用分层通信架构，各层协议定义严格一致：

- **USB CDC层**：树莓派 ↔ Pico2，帧格式包含时间戳和CRC32校验
- **SPI层**：Pico2 ↔ Pico，20Mbps高速数据传输
- **I2C调度层**：Pico2 ↔ Pico，400kHz硬件确定性同步

所有协议常量（命令码、数据类型、寄存器地址）在SDK版和Arduino版中完全一致。

## 版本对比

| 特性 | Demo版 | 完整版 |
|------|--------|--------|
| Pico2数量 | 1台 | 8台 |
| Pico数量 | 8台 | 128台 |
| 模拟通道 | 32路 | 512路 |
| 数字通道 | 64路 | 1024路 |
| 集群管理 | 基础 | 自适应扩展 |
| 向量化优化 | 无 | 有 (20-258x加速) |
| I2C调度总线 | 无 | 有 |
| DMA SPI收割 | 无 | 有 |

## 许可证

MIT License - 详见 [LICENSE](LICENSE)

## 子项目

- [总设计说明](DESIGN.md) - 完整的系统设计文档
- [树莓派主控层](sdk/SB/raspberry_pi/README.md)
- [Pico SDK 完整版](sdk/SB/README.md)
- [Arduino 版本](ide/arduino/README.md)
