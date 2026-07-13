# 超采集算系统 - 完整版 (SB) V1.4

Super Collection Compute System - Full Version with Adaptive Cluster

## 概述

超采集算系统是一套三层分布式架构的高性能信号采集与计算平台，支持自适应集群扩展和多种工作模式。**完整版**支持最多8个Pico2协处理器和128个Pico终端节点，并提供向量化优化算法、并行处理框架和智能自动识别功能。

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

### 自适应集群扩展
- **动态节点检测**：自动识别接入的Pico2和Pico节点
- **故障自动恢复**：检测到故障节点自动复位重连
- **负载均衡调度**：根据节点利用率智能分配任务
- **热插拔支持**：支持运行中增减节点
- **弹性扩展**：最多支持8个Pico2、128个Pico节点
- **SPI片选线自动地址分配**：物理槽位=逻辑地址，统一固件即插即用

### 多模式调度系统
- **采样模式**：512路模拟 + 1024路数字同步采集
- **破译模式**：边信道分析与协议破译
- **暴力破解**：分布式算力并行破解
- **硬件测试**：全面的硬件功能测试
- **模式切换流程**：停止→复位→配置→启动→状态同步

### 高性能算法引擎
- **向量化优化**：使用NumPy批量处理，FIR滤波加速258倍
- **并行处理框架**：多进程/多线程并行计算
- **流式处理**：持续数据输入，批量并行处理
- **内存可控**：固定缓冲区，无内存泄漏风险

### 智能自动识别
- **模拟信号识别**：自动识别正弦波、方波、三角波、直流、噪声、脉冲
- **协议自动识别**：UART波特率估计、SPI/I2C协议判别
- **哈希类型识别**：自动判别MD5/SHA-1/SHA-256/SHA-512等

### 高性能采集
- 模拟采样：12位分辨率，最高125KSPS
- 数字捕获：最高100MSPS采样率
- 数据聚合：Pico2层实时数据聚合
- 实时传输：USB CDC 100Mbps上传

### DMA SPI收割架构
- **DMA循环收割**：16个DMA通道并行收割，无需CPU干预
- **环形缓冲区**：每个Pico独立1KB环形缓冲区，自动溢出保护
- **事件通知**：DMA完成中断触发，CPU仅在数据就绪时唤醒
- **边缘计算**：在Pico2上完成Delta压缩和特征提取
- **数据精简**：ADC均值/方差/峰值 + 数字边沿统计，数据量减少80%

### 可靠性设计
- 硬件看门狗：15秒超时，系统级保护
- 掉电保护：FRAM配置双备份，数据安全落盘
- 故障隔离：单节点故障不影响整体系统
- 进程监控：Watchdog守护进程，崩溃自动重启
- 温度保护：超频温度阈值动态调节

## 目录结构

```
SB/
├── raspberry_pi/              # 树莓派主控层
│   ├── main.py               # 系统主入口
│   ├── requirements.txt      # Python依赖
│   ├── core/                 # 核心模块
│   │   ├── config.py         # 系统配置
│   │   ├── logger.py         # 日志系统
│   │   ├── status_manager.py # 状态管理
│   │   └── usb_comm.py       # USB通信
│   ├── cluster/              # 集群管理
│   │   └── adaptive_cluster.py  # 自适应集群
│   ├── modes/                # 模式调度
│   │   └── mode_scheduler.py # 多模式调度器
│   ├── algorithms/           # 算法库
│   │   ├── __init__.py
│   │   ├── analog_algorithms.py       # 模拟信号处理算法
│   │   ├── digital_algorithms.py      # 数字信号处理算法
│   │   ├── crack_algorithms.py        # 密码破解算法
│   │   ├── optimized_algorithms.py    # 向量化优化算法+自动识别
│   │   └── parallel_processing.py     # 并行处理框架
│   ├── daemons/              # 守护进程
│   │   ├── watchdog_daemon.py    # 看门狗守护
│   │   ├── data_receiver.py      # 数据接收
│   │   ├── business_logic.py     # 业务逻辑(集成算法)
│   │   ├── storage_remote.py     # 存储与远程
│   │   ├── mode_scheduler.py     # 模式调度守护
│   │   ├── hmi_task.py           # 人机交互
│   │   └── crack_engine_daemon.py # 破解引擎守护
│   ├── services/             # 服务
│   │   └── web_service.py    # Web监控服务
│   └── web/                  # Web前端
│       ├── templates/        # HTML模板
│       └── static/           # 静态资源
│           ├── css/
│           └── js/
├── pico/                     # Pico终端层固件
│   └── firmware/
│       ├── CMakeLists.txt
│       ├── include/          # 头文件
│       │   ├── addr_assign_protocol.h  # 地址分配协议
│       │   ├── spi_addr_slave.h       # SPI地址从机
│       │   └── i2c_sched_slave.h      # I2C调度从机
│       └── src/
│           ├── spi_addr_slave.c       # SPI地址从机实现
│           └── i2c_sched_slave.c      # I2C调度从机实现
├── pico2/                    # Pico2协处理层固件
│   └── firmware/
│       ├── CMakeLists.txt
│       ├── include/
│       │   ├── addr_assign_protocol.h  # 地址分配协议
│       │   ├── addr_assigner.h         # 地址分配器
│       │   ├── dma_spi_harvester.h     # DMA SPI收割器
│       │   ├── edge_compute.h          # 边缘计算
│       │   └── i2c_sched_master.h      # I2C调度主机
│       └── src/
│           ├── addr_assigner.c         # 地址分配器实现
│           ├── dma_spi_harvester.c     # DMA SPI收割器实现
│           ├── edge_compute.c          # 边缘计算实现
│           └── i2c_sched_master.c      # I2C调度主机实现
├── hardware/                 # 硬件设计文档
├── tools/                    # 工具脚本
│   ├── deploy.py             # 部署工具
│   ├── verify_system.py      # 系统验证
│   ├── test_algorithms.py    # 算法验证测试
│   └── test_optimization.py  # 优化与自动识别测试
└── docs/                     # 文档
```

## 快速开始

### 环境要求

- 树莓派 4B/5 (或其他Linux主机)
- Python 3.8+
- Pico SDK (编译固件用)
- CMake 3.13+

### 安装依赖

```bash
cd SB/raspberry_pi
pip install -r requirements.txt
```

### 系统验证

```bash
cd SB/tools
python verify_system.py
```

### 优化验证

```bash
cd SB/tools
python test_optimization.py
```

### 部署系统

```bash
cd SB/tools
python deploy.py deploy
```

### 启动系统

**看门狗模式（推荐生产环境）：**
```bash
cd SB/raspberry_pi
python main.py --mode watchdog
```

**单进程调试模式：**
```bash
cd SB/raspberry_pi
python main.py --mode standalone --debug
```

### 访问Web界面

启动后访问：`http://<树莓派IP>:5000`

## 工作模式

### 1. 采样模式
- 多通道模拟信号实时采集
- 数字信号高速捕获
- 实时滤波处理（滑动平均/中值/FIR）
- FFT频谱实时分析
- 信号自动识别与分类
- 波形数据本地存储

### 2. 破译模式
- 边信道功耗分析
- 时序特征提取
- 协议逆向分析
- 智能模式识别
- UART/SPI/I2C协议自动解码

### 3. 暴力破解模式
- 分布式并行计算（多进程）
- 可配置密钥长度与字符集
- 支持MD5/SHA1/SHA256
- 实时进度监控
- 哈希类型自动识别

### 4. 硬件测试模式
- ADC通道校准
- GPIO功能测试
- 通信链路检测
- 温度与电压监控

## 算法引擎

### 基础算法库

| 算法类别 | 算法名称 | 功能 | 速度 |
|---------|---------|------|------|
| 模拟信号处理 | 滑动平均滤波 | 去除高频噪声 | ~2.2M samples/sec |
| | 中值滤波 | 去除脉冲噪声 | ~1.3M samples/sec |
| | FIR低通滤波 | 精确频率滤波 | ~175K samples/sec |
| | 峰值检测 | 检测信号峰值 | ~3M samples/sec |
| | 互相关分析 | 信号相关性分析 | ~1M samples/sec |
| 数字信号处理 | 边沿检测 | 上升/下降沿检测 | - |
| | UART解码器 | 9600-115200波特率 | - |
| | SPI解码器 | CPOL/CPHA可配置 | - |
| | I2C解码器 | 标准/快速模式 | - |
| | 脉宽测量 | 周期/占空比测量 | - |
| 密码破解 | SHA-256 | 哈希计算 | ~200K hashes/sec |
| | MD5掩码攻击 | 字典+掩码组合 | ~175K hashes/sec |
| | 分布式分片 | 多节点任务分配 | - |

### 向量化优化（性能提升）

| 算法 | 原始速度 | 向量化速度 | 加速比 |
|------|---------|-----------|--------|
| 滑动平均(8阶) | ~2.2M samples/sec | ~43.9M samples/sec | **20x** |
| FIR滤波(31阶) | ~175K samples/sec | ~33.5M samples/sec | **258x** |

### 并行处理框架

| 组件 | 功能 | 模式 |
|------|------|------|
| ParallelProcessor | 通用并行处理器 | 进程/线程 |
| MultiChannelProcessor | 多通道并行处理 | 进程 |
| BatchStreamProcessor | 流式批量处理 | 线程 |
| CrackParallelEngine | 并行破解引擎 | 进程 |

### 自动识别功能

**模拟信号识别**
- 基于统计特征（峭度、波峰因数、周期、频谱等）
- 支持：直流、正弦波、方波、三角波、噪声、脉冲
- 置信度评估 + 分类间隔判断

**哈希类型识别**
- 根据长度和格式自动判别：MD5(32)/SHA-1(40)/SHA-256(64)/SHA-512(128)
- 支持特殊格式：bcrypt、pbkdf2、argon2、sha256_crypt等

**协议自动识别**
- 波特率自动估计
- 协议类型判别：UART、SPI、I2C、GPIO

## 集群管理

### 节点扩展
系统支持动态节点扩展，最大配置：

| 项目 | 配置 |
|------|------|
| Pico2协处理器 | 8台 |
| Pico终端节点 | 128台 (每Pico2 16台) |
| 模拟通道 | 512路 |
| 数字通道 | 1024路 |

### 自动发现
接入新节点后系统自动检测并加入集群，无需重启。

### 故障自愈
- 节点超时自动标记离线
- 自动尝试复位重连
- 故障节点任务自动迁移

## 技术规格

### 模拟采集
- 分辨率：12位
- 采样率：最高125KSPS (单通道)
- 通道数：4路/节点
- 输入范围：0 - 3.3V
- 精度：±1LSB

### 数字捕获
- 通道数：8路/节点
- 最高采样率：100MSPS
- 电平标准：3.3V CMOS
- 触发模式：边沿/电平/模式

### 通信接口

#### 数据平面（SPI）
- Pico ↔ Pico2：SPI 20Mbps，高速采样数据传输

#### 控制平面（I2C调度总线）
- Pico ↔ Pico2：I2C 400kHz，硬件确定性调度
- 从机地址：0x40 - 0x4F（16个Pico节点）
- 广播地址：0x00（所有节点同步）
- 寄存器数量：16个8位寄存器

#### 上行通信
- Pico2 ↔ 树莓派：USB CDC 100Mbps
- Web监控：WebSocket实时推送

### I2C调度总线寄存器映射

| 地址 | 名称 | 读写 | 说明 |
|------|------|------|------|
| 0x00 | CTRL | RW | 控制寄存器（运行/同步/复位/停止） |
| 0x01 | MODE | RW | 工作模式（采样/破译/暴力破解/硬件测试） |
| 0x02 | RATE | RW | 采样率编码（1K~125KHz） |
| 0x03 | ADC_CH | RW | ADC通道选择（4路） |
| 0x04 | DIG_CH | RW | 数字通道选择（8路） |
| 0x05 | SYNC | WO | 同步触发（写入0xAA触发同步） |
| 0x06 | STATUS | RO | 状态寄存器（运行/数据就绪/同步锁定） |
| 0x07 | ERROR | RO | 错误寄存器（ADC超时/通信错误/过热） |
| 0x08 | DATA_LEN | RO | 数据长度 |
| 0x09 | DATA_PTR | RO | 数据指针 |
| 0x0A | CHECKSUM | RO | 寄存器校验和 |
| 0x0B | VERSION | RO | 固件版本 |
| 0x0C | NODE_ID | RO | 节点ID（0-15） |
| 0x0D | CLUSTER_ID | RO | 集群ID（0-7） |
| 0x0E | OVERCLOCK | RW | 超频控制（禁用/167MHz/200MHz/自动） |
| 0x0F | RESERVED | - | 保留 |

### 同步触发机制

1. **广播同步启动**：Pico2写入广播地址0x00，REG_SYNC=0xAA
2. **所有Pico同步响应**：硬件I2C中断触发，同步精度<1μs
3. **状态反馈**：Pico2轮询读取各节点STATUS寄存器
4. **同步锁定**：所有节点STATUS_SYNC_LOCKED位置1

### 超频能力
- Pico：最高200MHz (默认133MHz)
- Pico2：最高240MHz (默认150MHz)
- 温度保护：65°C阈值动态降频

## 8大核心守护进程

| 进程 | 功能 | 优先级 |
|------|------|--------|
| watchdog_daemon | 看门狗守护 + 进程监控 + 掉电处理 | 最高 |
| data_receiver | USB数据接收 + 帧校验 + 缓冲 | 高 |
| mode_scheduler | 模式切换调度 + GPIO监控 | 高 |
| business_logic | 信号处理 + 破解计算 + 协议分析 | 中 |
| crack_engine_daemon | 并行破解引擎 + 任务调度 | 中 |
| storage_remote | 数据存储 + 日志管理 + 预警 | 低 |
| hmi_task | OLED显示 + 按键扫描 + LED控制 | 低 |
| web_service | Web界面 + API + WebSocket | 低 |

## Demo版 vs 完整版对比

| 特性 | Demo版 | 完整版 |
|------|--------|--------|
| Pico2数量 | 1台 | 8台 |
| Pico数量 | 8台 | 128台 |
| 模拟通道 | 32路 | 512路 |
| 数字通道 | 64路 | 1024路 |
| 集群管理 | 基础 | 自适应扩展 |
| 向量化优化 | 无 | 有 (20-258x加速) |
| 并行处理 | 无 | 有 |
| 信号自动识别 | 基础 | 完整 |
| 协议自动识别 | 无 | 有 |
| 哈希类型识别 | 无 | 有 |
| I2C调度总线 | 无 | 有 (硬件确定性同步) |
| DMA SPI收割 | 无 | 有 (边缘计算+Delta压缩) |
| 自动地址分配 | 无 | 有 (SPI片选线即插即用) |

## 固件编译

### Pico 终端层固件

```bash
cd pico/firmware
mkdir build && cd build
cmake -G Ninja .. -DPICO_SDK_PATH=/path/to/pico-sdk
ninja
```

生成文件：`super_calc_pico.uf2`

### Pico2 协处理层固件

```bash
cd pico2/firmware
mkdir build && cd build
cmake -G Ninja .. -DPICO_SDK_PATH=/path/to/pico-sdk
ninja
```

生成文件：`super_calc_pico2.uf2`

### 烧录方法

1. 按住BOOTSEL按钮连接USB
2. 设备识别为U盘
3. 复制`.uf2`文件到U盘
4. 自动重启运行

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

## USB CDC 帧格式

### 上行帧（Pico2 → 树莓派）

| 偏移 | 长度 | 字段 | 说明 |
|------|------|------|------|
| 0 | 1 | HEADER | 帧头 0x55 |
| 1 | 1 | NODE_ID | Pico2节点ID |
| 2 | 2 | DATA_LEN | 数据长度（小端） |
| 4 | 4 | TIMESTAMP | 时间戳（微秒） |
| 8 | n | DATA | 数据内容 |
| 8+n | 4 | CRC32 | 校验和（覆盖TIMESTAMP+DATA） |
| 12+n | 1 | TAIL | 帧尾 0xAA |

### 命令帧（树莓派 → Pico2）

| 偏移 | 长度 | 字段 | 说明 |
|------|------|------|------|
| 0 | 1 | HEADER | 帧头 0x55 |
| 1 | 1 | CMD | 命令码 |
| 2 | 2 | PARAM_LEN | 参数长度 |
| 4 | n | PARAM | 参数数据 |
| 4+n | 1 | TAIL | 帧尾 0xAA |

### 命令码定义

| 命令码 | 名称 | 功能 |
|--------|------|------|
| 0x00 | CMD_NOP | 空操作 |
| 0x01 | CMD_GET_STATUS | 获取状态 |
| 0x02 | CMD_START_SAMPLE | 开始采样 |
| 0x03 | CMD_STOP_SAMPLE | 停止采样 |
| 0x04 | CMD_SET_RATE | 设置采样率 |
| 0x05 | CMD_GET_DATA | 获取数据 |
| 0x06 | CMD_SET_MODE | 设置模式 |
| 0x07 | CMD_GET_VERSION | 获取版本 |
| 0x08 | CMD_OVERCLOCK | 超频控制 |
| 0x09 | CMD_HW_TEST | 硬件测试 |
| 0x0A | CMD_GLITCH | 毛刺注入 |
| 0x0B | CMD_NODE_DETECT | 节点检测 |
| 0x0C | CMD_SET_NODE_COUNT | 设置节点数 |
| 0x0D | CMD_RESET_NODE | 复位节点 |
| 0x0E | CMD_BROADCAST | 广播命令 |
| 0x0F | CMD_START_CRACK | 开始破解 |
| 0x10 | CMD_START_TEST | 开始测试 |
| 0x11 | CMD_ADDR_ASSIGN | 地址分配 |
| 0x12 | CMD_ADDR_QUERY | 地址查询 |
| 0x13 | CMD_ADDR_CLEAR | 清除地址 |
| 0xFF | CMD_RESET | 系统复位 |

## 协议一致性

本系统采用分层通信架构，各层协议定义严格一致：

- **USB CDC层**：树莓派 ↔ Pico2，帧格式包含时间戳和CRC32校验
- **SPI层**：Pico2 ↔ Pico，20Mbps高速数据传输
- **I2C调度层**：Pico2 ↔ Pico，400kHz硬件确定性同步

所有协议常量（命令码、数据类型、寄存器地址）在SDK版和Arduino版中完全一致。

## 许可证

MIT License - 详见 [LICENSE](../../../LICENSE)

## 版本历史

- **V1.0** - 初始完整版，支持自适应集群与多模式
- **V1.1** - 添加向量化优化算法、并行处理框架、智能自动识别功能
- **V1.2** - 添加I2C专用调度总线，实现硬件确定性同步
- **V1.3** - 添加DMA SPI收割架构，实现边缘计算与数据精简
- **V1.4** - 添加SPI片选线自动地址分配，统一固件即插即用
