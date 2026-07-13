# 超采集算系统 - 总设计说明

Super Collection Compute System - Complete Design Specification

---

## 1. 系统概述

超采集算系统是一套三层分布式架构的高性能信号采集与计算平台，专为大规模并行数据采集和边缘计算设计。系统采用 **树莓派作为主控层**、**Pico2 (RP2350) 作为协处理层**、**Pico (RP2040) 作为终端采集层**，实现从模拟/数字信号采集到边缘计算的完整链路。

### 设计目标

| 目标 | 指标 |
|------|------|
| 模拟通道数 | 512路（128节点 × 4通道） |
| 数字通道数 | 1024路（128节点 × 8通道） |
| 模拟采样率 | 最高125KSPS/通道 |
| 数字采样率 | 最高100MSPS（破译模式） |
| 同步精度 | <1μs（I2C硬件同步） |
| 数据传输 | USB CDC 100Mbps × 8链路 |

---

## 2. 系统架构

### 2.1 三层架构图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          树莓派 4B/5 主控层 (Master)                         │
│  ┌─────────────┬─────────────┬─────────────┬─────────────────────────────┐  │
│  │ Web 服务     │ 模式调度器   │ 集群管理器   │ 8大守护进程                  │  │
│  │ REST API    │ Mode Scheduler│ Cluster Mgr│                              │  │
│  │              │             │             │ 算法引擎 (向量化+并行)        │  │
│  │              │             │             │ USB通信管理                  │  │
│  │              │             │             │ 数据存储与分析               │  │
│  └─────────────┴─────────────┴─────────────┴─────────────────────────────┘  │
└───────────────────────────────┬─────────────────────────────────────────────┘
                                │ USB CDC (100Mbps) × 8
┌───────────────────────────────┴─────────────────────────────────────────────┐
│                     Pico2 (RP2350) 协处理层 (Co-processor) × 8             │
│  ┌─────────────┬─────────────┬─────────────┬─────────────────────────────┐  │
│  │数据聚合器    │ SPI主控器    │ I2C调度器    │ 状态管理器                  │  │
│  │Data Aggregator│ SPI Master │ I2C Scheduler│ System Status              │  │
│  │              │             │             │ 超频控制器                  │  │
│  │              │             │             │ 防呆检测                    │  │
│  │              │             │             │ 地址分配器                  │  │
│  └─────────────┴─────────────┴─────────────┴─────────────────────────────┘  │
└───────────────────────────────┬─────────────────────────────────────────────┘
                                │ SPI (20Mbps) × 16 + I2C (400kHz) × 16
┌───────────────────────────────┴─────────────────────────────────────────────┐
│                      Pico (RP2040) 终端层 (Terminal) × 128                  │
│  ┌─────────────┬─────────────┬─────────────┬─────────────────────────────┐  │
│  │ADC采样器    │ 数字捕获器   │ 破解引擎     │ 硬件测试器                  │  │
│  │ADC Sampler  │ Digital Capture│ Crack Engine│ HW Tester                  │  │
│  │             │             │             │ 温度监控                    │  │
│  │             │             │             │ I2C从机                     │  │
│  │             │             │             │ SPI从机                     │  │
│  └─────────────┴─────────────┴─────────────┴─────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 各层职责

| 层级 | 硬件 | 职责 | 数量 |
|------|------|------|------|
| **主控层** | 树莓派 4B/5 | Web界面、模式调度、集群管理、算法引擎 | 1 |
| **协处理层** | Pico2 (RP2350) | 数据聚合、SPI主控、I2C调度、超频控制 | 8 |
| **终端层** | Pico (RP2040) | ADC采样、数字捕获、破解计算、硬件测试 | 128 |

---

## 3. 硬件设计

### 3.1 Pico (RP2040) 终端层引脚分配

| 功能 | GPIO | 方向 | 复用功能 | 说明 |
|------|------|------|----------|------|
| SPI SCK | GP2 | IN | SPI0 SCK | 数据平面时钟 |
| SPI MOSI | GP3 | IN | SPI0 MOSI | 数据平面输入 |
| SPI MISO | GP4 | OUT | SPI0 MISO | 数据平面输出 |
| SPI CS | GP1 | IN | SPI0 CSn | 片选（由Pico2控制） |
| I2C SDA | GP0 | IN/OUT | I2C0 SDA | 控制平面数据线 |
| I2C SCL | GP5 | IN | I2C0 SCL | 控制平面时钟线 |
| ADC 0 | GP26 | IN | ADC0 | 模拟输入通道0 |
| ADC 1 | GP27 | IN | ADC1 | 模拟输入通道1 |
| ADC 2 | GP28 | IN | ADC2 | 模拟输入通道2 |
| ADC 3 | GP29 | IN | ADC3 | 模拟输入通道3 |
| DIG 0-3 | GP6-GP9 | IN | PIO0 | 数字捕获通道（采样模式） |
| DIG 0-7 | GP0-GP7 | IN | PIO0 | 数字捕获通道（破译模式） |
| PRSNT# | GP22 | IN | GPIO | PCIe卡检测引脚（低电平有效） |
| RUN_LED | GP25 | OUT | GPIO | 运行指示灯 |

**注意**：GP0-GP7 在采样模式和破译模式间动态切换，需先停止 I2C/SPI 再切换。

### 3.2 Pico2 (RP2350) 协处理层引脚分配

| 功能 | GPIO | 方向 | 复用功能 | 说明 |
|------|------|------|----------|------|
| SPI1 SCK | GP10 | OUT | SPI1 SCK | 数据平面时钟 |
| SPI1 MOSI | GP11 | OUT | SPI1 MOSI | 数据平面输出 |
| SPI1 MISO | GP12 | IN | SPI1 MISO | 数据平面输入 |
| SPI1 CS0 | GP2 | OUT | SPI1 CS0 | Pico #0 片选 |
| SPI1 CS1 | GP3 | OUT | SPI1 CS1 | Pico #1 片选 |
| SPI1 CS2 | GP4 | OUT | SPI1 CS2 | Pico #2 片选 |
| SPI1 CS3 | GP5 | OUT | SPI1 CS3 | Pico #3 片选 |
| SPI1 CS4 | GP6 | OUT | SPI1 CS4 | Pico #4 片选 |
| SPI1 CS5 | GP7 | OUT | SPI1 CS5 | Pico #5 片选 |
| SPI1 CS6 | GP8 | OUT | SPI1 CS6 | Pico #6 片选 |
| SPI1 CS7 | GP9 | OUT | SPI1 CS7 | Pico #7 片选 |
| SPI1 CS8 | GP13 | OUT | SPI1 CS8 | Pico #8 片选 |
| SPI1 CS9 | GP14 | OUT | SPI1 CS9 | Pico #9 片选 |
| SPI1 CS10 | GP15 | OUT | SPI1 CS10 | Pico #10 片选 |
| SPI1 CS11 | GP16 | OUT | SPI1 CS11 | Pico #11 片选 |
| SPI1 CS12 | GP17 | OUT | SPI1 CS12 | Pico #12 片选 |
| SPI1 CS13 | GP18 | OUT | SPI1 CS13 | Pico #13 片选 |
| SPI1 CS14 | GP19 | OUT | SPI1 CS14 | Pico #14 片选 |
| SPI1 CS15 | GP20 | OUT | SPI1 CS15 | Pico #15 片选 |
| I2C0 SDA | GP26 | IN/OUT | I2C0 SDA | 控制平面数据线 |
| I2C0 SCL | GP27 | OUT | I2C0 SCL | 控制平面时钟线 |
| PRSNT# | GP21 | IN | GPIO | PCIe卡检测引脚（低电平有效） |
| FAULT_LED | GP22 | OUT | GPIO | 故障红灯（高电平点亮） |
| RUN_LED | GP25 | OUT | GPIO | 运行指示灯 |

### 3.3 连接拓扑

```
树莓派 4B/5                          Pico2 #0 (RP2350)
    │                                    │
    ├── USB-A 0 ── Pico2 #0             ├── SPI1 CS0 ── Pico #00 (GP1)
    ├── USB-A 1 ── Pico2 #1             ├── SPI1 CS1 ── Pico #01 (GP1)
    ├── USB-A 2 ── Pico2 #2             ├── SPI1 CS2 ── Pico #02 (GP1)
    ├── USB-A 3 ── Pico2 #3             ├── SPI1 CS3 ── Pico #03 (GP1)
    ├── USB-A 4 ── Pico2 #4             ├── SPI1 CS4 ── Pico #04 (GP1)
    ├── USB-A 5 ── Pico2 #5             ├── SPI1 CS5 ── Pico #05 (GP1)
    ├── USB-A 6 ── Pico2 #6             ├── SPI1 CS6 ── Pico #06 (GP1)
    └── USB-A 7 ── Pico2 #7             ├── SPI1 CS7 ── Pico #07 (GP1)
                                        ├── SPI1 CS8 ── Pico #08 (GP1)
                                        ├── SPI1 CS9 ── Pico #09 (GP1)
                                        ├── SPI1 CS10 ── Pico #10 (GP1)
                                        ├── SPI1 CS11 ── Pico #11 (GP1)
                                        ├── SPI1 CS12 ── Pico #12 (GP1)
                                        ├── SPI1 CS13 ── Pico #13 (GP1)
                                        ├── SPI1 CS14 ── Pico #14 (GP1)
                                        └── SPI1 CS15 ── Pico #15 (GP1)
```

### 3.4 供电方案

| 设备 | 电压 | 典型电流 | 总功率（满配） |
|------|------|----------|----------------|
| Pico (RP2040) | 3.3V | ~30mA | ~4W（128台） |
| Pico2 (RP2350) | 3.3V | ~50mA | ~1.3W（8台） |
| 树莓派 4B | 5V | ~500mA | ~2.5W |
| **总计** | - | - | **~7.8W** |

---

## 4. 软件架构

### 4.1 FreeRTOS 任务模型（Pico2）

| 任务 | 优先级 | 栈大小 | 周期 | 职责 |
|------|--------|--------|------|------|
| Foolproof | 6 (最高) | 128 | 10ms | 防呆检测、PRSNT#检测、故障红灯 |
| SPI Master | 5 | 256 | 1ms | SPI数据传输、节点通信 |
| Data Aggregator | 4 | 512 | 1ms | 数据聚合、Delta压缩 |
| USB CDC | 4 | 512 | 2ms | USB数据收发、帧解析 |
| Temperature Monitor | 2 | 128 | 100ms | 温度监控、超频控制 |

### 4.2 FreeRTOS 任务模型（Pico）

| 任务 | 优先级 | 栈大小 | 周期 | 职责 |
|------|--------|--------|------|------|
| SPI Comm | 4 | 256 | 1ms | SPI命令接收、响应发送 |
| Sample | 3 | 256 | 1ms | ADC采样、数字捕获 |
| Crack | 2 | 512 | - | 破译计算、暴力破解 |
| Monitor | 1 | 128 | 100ms | 温度监控、状态上报 |

### 4.3 模块依赖关系

```
树莓派主控层
    ├── core/          # 核心模块
    │   ├── usb_comm.py       # USB CDC通信
    │   ├── config.py         # 配置管理
    │   ├── frame_parser.py   # 帧解析
    │   └── crc32.py          # CRC32校验
    ├── scheduler/     # 调度模块
    │   ├── mode_scheduler.py # 模式调度
    │   └── cluster_manager.py # 集群管理
    ├── engine/        # 算法引擎
    │   ├── vector_engine.py  # 向量化引擎
    │   └── parallel_engine.py # 并行引擎
    └── web/           # Web服务

Pico2协处理层
    ├── spi_master.c       # SPI主控
    ├── i2c_sched_master.c # I2C调度主控
    ├── data_aggregator.c  # 数据聚合
    ├── usb_cdc_comm.c     # USB CDC通信
    ├── edge_compute.c     # 边缘计算
    ├── addr_assigner.c    # 地址分配
    ├── foolproof.c        # 防呆检测
    └── oc_control.c       # 超频控制

Pico终端层
    ├── adc_sample.c       # ADC采样
    ├── digital_capture.c  # 数字捕获
    ├── spi_comm.c         # SPI从机通信
    ├── i2c_sched_slave.c  # I2C调度从机
    ├── crack_engine.c     # 破解引擎
    ├── foolproof.c        # 防呆检测
    └── oc_control.c       # 超频控制
```

---

## 5. 通信协议

### 5.1 USB CDC 帧格式（树莓派 ↔ Pico2）

```
┌────────┬────────┬────────┬────────┬──────────┬──────────┬────────┐
│ Header │ NodeID │ Length │ Time   │ Data     │ CRC32    │ Tail   │
│ (1B)   │ (1B)   │ (2B)   │ (4B)   │ (N B)    │ (4B)     │ (1B)   │
└────────┴────────┴────────┴────────┴──────────┴──────────┴────────┘
  0x55      N      LSB     UTC      可变长度   覆盖时间戳   0xAA
                      MSB            +数据
```

| 字段 | 大小 | 说明 |
|------|------|------|
| Header | 1字节 | 帧头，固定为 `0x55` |
| NodeID | 1字节 | 节点ID，0-7对应8台Pico2 |
| Length | 2字节 | 数据长度（小端序） |
| Time | 4字节 | UTC时间戳（微秒级） |
| Data | N字节 | 有效数据 |
| CRC32 | 4字节 | 覆盖 Time + Data 字段 |
| Tail | 1字节 | 帧尾，固定为 `0xAA` |

### 5.2 SPI 帧格式（Pico2 ↔ Pico）

```
┌────────┬────────┬────────┬────────┬──────────┬────────┐
│ Header │ Length │ Command│ Params │ CRC16    │ Tail   │
│ (1B)   │ (2B)   │ (1B)   │ (N B)  │ (2B)     │ (1B)   │
└────────┴────────┴────────┴────────┴──────────┴────────┘
  0xAA      LSB     CMD     可变长度 覆盖命令+参数  0x55
            MSB
```

| 字段 | 大小 | 说明 |
|------|------|------|
| Header | 1字节 | 帧头，固定为 `0xAA` |
| Length | 2字节 | 命令+参数长度（小端序） |
| Command | 1字节 | 命令码 |
| Params | N字节 | 参数数据 |
| CRC16 | 2字节 | 覆盖 Command + Params 字段 |
| Tail | 1字节 | 帧尾，固定为 `0x55` |

### 5.3 I2C 寄存器映射

#### 寄存器地址定义

| 地址 | 名称 | 读写 | 说明 |
|------|------|------|------|
| 0x00 | REG_CTRL | RW | 控制寄存器 |
| 0x01 | REG_MODE | RW | 工作模式 |
| 0x02 | REG_SAMPLE_RATE | RW | 采样率配置 |
| 0x03 | REG_ADC_CH_SEL | RW | ADC通道选择 |
| 0x04 | REG_DIG_CH_SEL | RW | 数字通道选择 |
| 0x05 | REG_SYNC_TRIGGER | WO | 同步触发（写触发） |
| 0x06 | REG_STATUS | RO | 状态寄存器 |
| 0x07 | REG_ERROR | RO | 错误寄存器 |
| 0x08 | REG_SYNC_TIME_0 | RO | 同步时间戳低字节 |
| 0x09 | REG_SYNC_TIME_1 | RO | 同步时间戳 |
| 0x0A | REG_SYNC_TIME_2 | RO | 同步时间戳 |
| 0x0B | REG_SYNC_TIME_3 | RO | 同步时间戳高字节 |
| 0x0C | REG_CHECKSUM | RO | 寄存器校验和 |
| 0x0D | REG_VERSION | RO | 固件版本 |
| 0x0E | REG_NODE_ID | RW | 节点ID |
| 0x0F | REG_CLUSTER_ID | RW | 集群ID |
| 0x10 | REG_OVERCLOCK | RW | 超频模式 |
| 0x11-0x1F | REG_RESERVED | - | 保留 |

#### 控制寄存器位定义（REG_CTRL）

| 位 | 掩码 | 名称 | 说明 |
|----|------|------|------|
| 0 | 0x01 | CTRL_RUN | 开始采样 |
| 1 | 0x02 | CTRL_SYNC | 触发同步 |
| 2 | 0x04 | CTRL_RESET | 复位 |
| 3 | 0x08 | CTRL_STOP | 停止采样 |
| 4 | 0x10 | CTRL_CLEAR_ERR | 清除错误 |
| 5 | 0x20 | CTRL_AUTO_RESTART | 自动重启 |
| 7 | 0x80 | CTRL_BROADCAST | 广播模式 |

#### 状态寄存器位定义（REG_STATUS）

| 位 | 掩码 | 名称 | 说明 |
|----|------|------|------|
| 0 | 0x01 | STATUS_RUNNING | 运行中 |
| 1 | 0x02 | STATUS_DATA_READY | 数据就绪 |
| 2 | 0x04 | STATUS_SYNC_LOCKED | 同步锁定 |
| 3 | 0x08 | STATUS_OVERCLOCKED | 超频中 |
| 4 | 0x10 | STATUS_HW_TEST_PASS | 硬件测试通过 |
| 5 | 0x20 | STATUS_BUSY | 忙 |
| 6 | 0x40 | STATUS_WATCHDOG_OK | 看门狗正常 |
| 7 | 0x80 | STATUS_INIT_COMPLETE | 初始化完成 |

#### 错误寄存器位定义（REG_ERROR）

| 位 | 掩码 | 名称 | 说明 |
|----|------|------|------|
| 0 | 0x01 | ERR_ADC_TIMEOUT | ADC超时 |
| 1 | 0x02 | ERR_DIG_TIMEOUT | 数字捕获超时 |
| 2 | 0x04 | ERR_SPI_COMM | SPI通信错误 |
| 3 | 0x08 | ERR_I2C_COMM | I2C通信错误 |
| 4 | 0x10 | ERR_OVERHEAT | 过热 |
| 5 | 0x20 | ERR_OVERCLOCK_FAIL | 超频失败 |
| 6 | 0x40 | ERR_DATA_CORRUPT | 数据损坏 |
| 7 | 0x80 | ERR_UNKNOWN | 未知错误 |

### 5.4 命令码定义

| 命令码 | 名称 | 方向 | 说明 |
|--------|------|------|------|
| 0x00 | CMD_NOP | Pico2→Pico | 空操作 |
| 0x01 | CMD_GET_STATUS | Pico2→Pico | 获取状态 |
| 0x02 | CMD_START_SAMPLE | Pico2→Pico | 开始采样 |
| 0x03 | CMD_STOP_SAMPLE | Pico2→Pico | 停止采样 |
| 0x04 | CMD_SET_RATE | Pico2→Pico | 设置采样率 |
| 0x05 | CMD_GET_DATA | Pico2→Pico | 获取数据 |
| 0x06 | CMD_SET_MODE | Pico2→Pico | 设置模式 |
| 0x07 | CMD_GET_VERSION | Pico2→Pico | 获取版本 |
| 0x08 | CMD_OVERCLOCK | Pico2→Pico | 超频控制 |
| 0x09 | CMD_HW_TEST | Pico2→Pico | 硬件测试 |
| 0x0A | CMD_GLITCH | Pico2→Pico | 故障注入 |
| 0x0B | CMD_NODE_DETECT | Pico2→Pico | 节点检测 |
| 0x0C | CMD_SET_NODE_COUNT | Pico2→Pico | 设置节点数 |
| 0x0D | CMD_RESET_NODE | Pico2→Pico | 复位节点 |
| 0x0E | CMD_BROADCAST | Pico2→Pico | 广播命令 |
| 0x0F | CMD_START_CRACK | Pico2→Pico | 开始破译 |
| 0x10 | CMD_START_TEST | Pico2→Pico | 开始测试 |
| 0x11 | CMD_ADDR_ASSIGN | Pico2→Pico | 地址分配 |
| 0x12 | CMD_ADDR_QUERY | Pico2→Pico | 地址查询 |
| 0x13 | CMD_ADDR_CLEAR | Pico2→Pico | 地址清除 |
| 0xFF | CMD_RESET | Pico2→Pico | 全局复位 |

### 5.5 数据类型定义

| 类型码 | 名称 | 说明 |
|--------|------|------|
| 0x01 | DATA_ANALOG | 模拟采样数据 |
| 0x02 | DATA_DIGITAL | 数字捕获数据 |
| 0x03 | DATA_STATUS | 状态数据 |
| 0x04 | DATA_CRACK | 破译结果数据 |
| 0x10 | DATA_AGGREGATED | 聚合数据 |
| 0x11 | DATA_CLUSTER_INFO | 集群信息 |
| 0x12 | DATA_FAULT | 故障信息 |

---

## 6. 工作模式

### 6.1 模式定义

| 模式码 | 名称 | 说明 | 引脚配置 |
|--------|------|------|----------|
| 0x00 | MODE_SAMPLE | 采样模式 | DIG使用GP6-GP9 |
| 0x01 | MODE_CRACK | 破译模式 | DIG使用GP0-GP7（需停止I2C/SPI） |
| 0x02 | MODE_BRUTEFORCE | 暴力破解模式 | 同上，启用破解引擎 |
| 0x03 | MODE_HW_TEST | 硬件测试模式 | 运行自检流程 |
| 0x04 | MODE_DIAGNOSTIC | 诊断模式 | 详细状态上报 |

### 6.2 模式切换流程

```
采样模式 → 破译模式
    1. 停止采样（CTRL_STOP）
    2. 停止I2C从机（i2c_slave_deinit）
    3. 重新初始化PIO，使用GP0-GP7
    4. 启动数字捕获

破译模式 → 采样模式
    1. 停止数字捕获
    2. 重新初始化GPIO功能（GP0=I2C SDA, GP1=SPI CS, GP2-4=SPI）
    3. 重启I2C从机（i2c_slave_init）
    4. 启动采样（CTRL_RUN）
```

### 6.3 采样率配置

| 速率码 | 名称 | 频率 |
|--------|------|------|
| 0x00 | RATE_1KHZ | 1 kHz |
| 0x01 | RATE_5KHZ | 5 kHz |
| 0x02 | RATE_10KHZ | 10 kHz |
| 0x03 | RATE_25KHZ | 25 kHz |
| 0x04 | RATE_50KHZ | 50 kHz |
| 0x05 | RATE_100KHZ | 100 kHz |
| 0x06 | RATE_125KHZ | 125 kHz |

---

## 7. 安全机制

### 7.1 防呆检测

| 检测项 | 引脚 | 条件 | 处理 |
|--------|------|------|------|
| PRSNT#检测 | GP22 (Pico) / GP21 (Pico2) | 高电平=未插入 | 点亮故障灯 |
| 温度监控 | ADC通道4 | ≥70°C=过热 | 紧急停止 |
| 节点离线 | SPI通信 | 连续10次超时 | 标记故障节点 |
| SPI通信 | SPI接口 | CRC校验失败 | 重传或标记错误 |
| USB通信 | USB CDC | 帧头/尾不匹配 | 丢弃帧 |

### 7.2 看门狗

| 参数 | 值 | 说明 |
|------|------|------|
| Pico超时 | 8秒 | 采样任务超时复位 |
| Pico2超时 | 10秒 | 防呆检测超时复位 |

### 7.3 温度保护

| 阈值 | 温度 | 处理 |
|------|------|------|
| 警告 | 60°C | 禁用超频 |
| 停机 | 70°C | 紧急停止所有采样 |

### 7.4 超频控制

| 模式码 | 名称 | 频率 | 电压 |
|--------|------|------|------|
| 0x00 | OC_DISABLED | 133MHz (Pico) / 150MHz (Pico2) | 默认 |
| 0x01 | OC_167MHZ | 167MHz | 1.1V |
| 0x02 | OC_200MHZ | 200MHz (Pico) / 240MHz (Pico2) | 1.2V |
| 0x03 | OC_AUTO | 自动调节 | 自动 |

---

## 8. 地址分配

### 8.1 I2C地址映射

| 地址范围 | 用途 |
|----------|------|
| 0x3C | 广播地址 |
| 0x40-0x4F | Pico节点地址（16个节点/每个Pico2） |
| 0x50-0x77 | 预留 |

### 8.2 地址计算公式

```
Pico I2C地址 = I2C_ADDR_BASE + NodeID
            = 0x40 + NodeID

NodeID范围：0-15（每个Pico2管理16个Pico）
```

### 8.3 物理槽位映射

物理槽位号（SPI CS线编号）直接映射为逻辑地址，实现即插即用。

---

## 9. 目录结构

```
rp/
├── LICENSE                    # MIT许可证
├── .gitignore                 # Git忽略规则
├── README.md                  # 项目总览
├── DESIGN.md                  # 总设计说明（本文档）
├── sdk/                       # Pico SDK版本工程
│   ├── SB/                    # 完整版
│   │   ├── raspberry_pi/      # 树莓派主控层（Python）
│   │   │   ├── core/          # 核心模块
│   │   │   ├── scheduler/     # 调度模块
│   │   │   ├── engine/        # 算法引擎
│   │   │   ├── web/           # Web服务
│   │   │   └── README.md
│   │   ├── pico/              # Pico终端层固件（C）
│   │   │   ├── firmware/
│   │   │   │   ├── src/       # 源代码
│   │   │   │   ├── include/   # 头文件
│   │   │   │   └── CMakeLists.txt
│   │   ├── pico2/             # Pico2协处理层固件（C）
│   │   │   ├── firmware/
│   │   │   │   ├── src/
│   │   │   │   ├── include/
│   │   │   │   └── CMakeLists.txt
│   │   └── README.md
│   └── demo/                  # Demo版
├── ide/                       # Arduino IDE版本工程
│   └── arduino/
│       ├── pico_sb/           # Pico完整版
│       ├── pico2_sb/          # Pico2完整版
│       ├── pico_demo/         # Pico Demo版
│       ├── pico2_demo/        # Pico2 Demo版
│       └── README.md
└── hardware/                  # 硬件设计文档（可选）
```

---

## 10. 构建方法

### 10.1 Pico SDK 版本

**环境要求**：
- Pico SDK 2.x
- ARM GCC 交叉编译器（arm-none-eabi-gcc）
- CMake 3.13+
- Ninja 构建工具

**编译命令**：

```bash
# Pico终端层
cd sdk/SB/pico/firmware
mkdir build && cd build
cmake -G Ninja .. -DPICO_SDK_PATH=/path/to/pico-sdk
ninja

# Pico2协处理层
cd sdk/SB/pico2/firmware
mkdir build && cd build
cmake -G Ninja .. -DPICO_SDK_PATH=/path/to/pico-sdk
ninja
```

**烧录**：
- 将Pico/Pico2进入BOOTSEL模式
- 复制生成的 `.uf2` 文件到设备

### 10.2 Arduino 版本

**环境要求**：
- Arduino IDE 2.0+
- Raspberry Pi Pico/RP2040 板包
- Pico 2/RP2350 板包

**编译上传**：
1. 打开 Arduino IDE
2. 安装对应板包
3. 打开 `.ino` 文件
4. 选择开发板类型
5. 点击"上传"按钮

### 10.3 树莓派主控层

**环境要求**：
- Python 3.8+
- pip 包管理

**安装依赖**：
```bash
cd sdk/SB/raspberry_pi
pip install -r requirements.txt
```

**启动守护进程**：
```bash
python main.py --mode watchdog
```

---

## 11. 版本一致性

系统采用双版本架构（Pico SDK版和Arduino版），所有关键协议常量保持一致：

| 常量类别 | SDK版 | Arduino版 | 一致性 |
|----------|-------|-----------|--------|
| 命令码 | config.h | config.h | ✅ |
| 数据类型 | config.h | config.h | ✅ |
| 寄存器地址 | i2c_sched_regs.h | i2c_sched_slave.h | ✅ |
| 控制位掩码 | i2c_sched_regs.h | i2c_sched_slave.h | ✅ |
| 状态位掩码 | i2c_sched_regs.h | i2c_sched_slave.h | ✅ |
| 同步触发值 | 0xAA | 0xAA | ✅ |
| SPI配置 | CPOL=0, CPHA=0 | CPOL=0, CPHA=0 | ✅ |
| I2C频率 | 400kHz | 400kHz | ✅ |

---

## 12. 安全注意事项

1. **GPIO冲突**：破译模式使用GP0-GP7，会占用I2C SDA和SPI CS引脚，必须先停止相关外设
2. **温度保护**：超频时需监控温度，超过70°C自动降频
3. **看门狗**：所有任务必须在超时前喂狗，防止系统挂死
4. **CRC校验**：所有通信链路均带CRC校验，防止数据损坏
5. **防呆检测**：PRSNT#引脚检测确保硬件正确连接

---

## 附录：帧格式速查表

### USB CDC 帧（树莓派 → Pico2）

```
[0x55][NodeID][Len_L][Len_H][Time0][Time1][Time2][Time3][Data...][CRC0-3][0xAA]
```

### SPI 帧（Pico2 → Pico）

```
[0xAA][Len_L][Len_H][Cmd][Params...][CRC_L][CRC_H][0x55]
```

### I2C 寄存器读写

```
写：[SlaveAddr+W][RegAddr][Value]
读：[SlaveAddr+W][RegAddr][SlaveAddr+R][Value]
```

---

*文档版本：V1.0*  
*最后更新：2026年7月*  
*版权所有：超采集算系统开发团队*
