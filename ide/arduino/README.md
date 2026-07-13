# 超采集算系统 - Arduino 版本

Super Collection Compute System - Arduino IDE Version

## 概述

本目录包含超采集算系统的 Arduino IDE 版本固件，支持使用 Arduino IDE 进行开发和烧录，无需配置复杂的 CMake 构建环境。

## 项目结构

```
arduino/
├── pico_sb/              # Pico (RP2040) 完整版固件
│   ├── pico_sb.ino       # 主入口
│   └── src/              # 源文件目录
│       ├── adc_sample.*          # ADC采样模块
│       ├── crack_engine.*        # 破解引擎模块
│       ├── digital_capture.*     # 数字捕获模块
│       ├── hw_test.*             # 硬件测试模块
│       ├── i2c_sched_regs.*      # I2C寄存器定义
│       ├── i2c_sched_slave.*     # I2C从机通信
│       ├── overclock.*           # 超频控制
│       ├── spi_comm.*            # SPI通信
│       ├── status_mgr.*          # 状态管理
│       └── config.h              # 配置定义
├── pico2_sb/             # Pico2 (RP2350) 完整版固件
│   ├── pico2_sb.ino      # 主入口
│   └── src/              # 源文件目录
│       ├── addr_assigner.*       # 地址分配器
│       ├── cluster_mgr.*         # 集群管理
│       ├── data_aggregator.*     # 数据聚合
│       ├── dma_spi_harvester.*   # DMA SPI收割器
│       ├── edge_compute.*        # 边缘计算
│       ├── foolproof.*           # 防呆检测
│       ├── i2c_sched_master.*    # I2C主机通信
│       ├── oc_control.*          # 超频控制
│       ├── spi_master.*          # SPI主控
│       ├── system_status.*       # 系统状态
│       ├── usb_cdc_comm.*        # USB CDC通信
│       └── config.h              # 配置定义
├── pico_demo/            # Pico Demo版固件
│   └── pico_demo.ino
└── pico2_demo/           # Pico2 Demo版固件
    └── pico2_demo.ino
```

## 快速开始

### 环境要求

- Arduino IDE 2.0+
- Raspberry Pi Pico/RP2040 板包
- Raspberry Pi Pico 2/RP2350 板包

### 安装板包

1. 打开 Arduino IDE
2. 点击 `工具` → `开发板` → `开发板管理器`
3. 搜索并安装 `Raspberry Pi Pico/RP2040`
4. 搜索并安装 `Raspberry Pi Pico 2/RP2350`

### 配置开发板

#### Pico (RP2040)

- `工具` → `开发板` → `Raspberry Pi Pico/RP2040` → `Raspberry Pi Pico`
- `工具` → `USB Stack` → `TinyUSB`

#### Pico2 (RP2350)

- `工具` → `开发板` → `Raspberry Pi Pico 2/RP2350` → `Raspberry Pi Pico 2`
- `工具` → `USB Stack` → `TinyUSB`

### 编译与烧录

1. 打开对应的 `.ino` 文件
2. 选择正确的开发板和端口
3. 点击 `上传` 按钮编译并烧录

## 硬件接线

### Pico (RP2040) 引脚分配

| 功能 | GPIO | 说明 |
|------|------|------|
| SPI SCK | GP2 | 数据平面时钟 |
| SPI MOSI | GP3 | 数据平面输入 |
| SPI MISO | GP4 | 数据平面输出 |
| SPI CS | GP1 | 片选（由Pico2控制） |
| I2C SDA | GP0 | 控制平面数据线 |
| I2C SCL | GP5 | 控制平面时钟线 |
| ADC 0 | GP26 | 模拟输入通道0 |
| ADC 1 | GP27 | 模拟输入通道1 |
| ADC 2 | GP28 | 模拟输入通道2 |
| ADC 3 | GP29 | 模拟输入通道3 |
| DIG 0-3 | GP6-GP9 | 数字捕获通道（采样模式） |
| DIG 0-7 | GP0-GP7 | 数字捕获通道（破译模式） |
| PRSNT# | GP22 | PCIe卡检测引脚 |

### Pico2 (RP2350) 引脚分配

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

## I2C 寄存器映射

| 地址 | 名称 | 读写 | 说明 |
|------|------|------|------|
| 0x00 | CTRL | RW | 控制寄存器 |
| 0x01 | MODE | RW | 工作模式 |
| 0x02 | RATE | RW | 采样率编码 |
| 0x03 | ADC_CH | RW | ADC通道选择 |
| 0x04 | DIG_CH | RW | 数字通道选择 |
| 0x05 | SYNC | WO | 同步触发 |
| 0x06 | STATUS | RO | 状态寄存器 |
| 0x07 | ERROR | RO | 错误寄存器 |
| 0x08-0x0B | SYNC_TIME | WO | 同步时间戳（4字节） |
| 0x0C | CHECKSUM | RO | 校验和 |
| 0x0D | VERSION | RO | 固件版本 |
| 0x0E | NODE_ID | RO | 节点ID |
| 0x0F | RESERVED | - | 保留 |

## 协议一致性

Arduino 版本与 Pico SDK 版本使用完全一致的协议定义：

- 命令码、数据类型、寄存器地址完全一致
- USB CDC 帧格式完全一致（包含时间戳和CRC32）
- I2C 寄存器映射完全一致
- 同步触发机制完全一致（TRIGGER_ACTIVE=0xAA）

## 许可证

MIT License - 详见 [LICENSE](../../LICENSE)
