# 超采集算系统 - 通信协议文档

## 概述

本文档定义了超采集算系统中各组件之间的通信协议。

## 系统架构

```
树莓派4B (主控)
    │
    ├── USB CDC (100Mbps)
    │   ├── Pico2 #0 (协处理器)
    │   │   └── SPI (20Mbps)
    │   │       ├── Pico #0-#7 (终端芯片)
    │   │       └── ...
    │   └── Pico2 #1 (协处理器)
    │       └── SPI (20Mbps)
    │           ├── Pico #8-#15 (终端芯片)
    │           └── ...
    │
    └── HTTP/WebSocket (Web界面)
```

## 1. Pico → Pico2 (SPI通信)

### 1.1 帧结构

| 字段 | 长度 | 描述 |
|------|------|------|
| 帧头 | 1字节 | 固定值 0xAA |
| 长度 | 2字节 | 数据长度（小端格式） |
| 数据 | N字节 | 有效数据，最大1024字节 |
| CRC16 | 2字节 | CRC16校验（小端格式） |
| 帧尾 | 1字节 | 固定值 0x55 |

### 1.2 命令码定义

| 命令码 | 名称 | 参数 | 描述 |
|--------|------|------|------|
| 0x00 | CMD_NOP | 无 | 空操作 |
| 0x01 | CMD_GET_STATUS | 无 | 获取节点状态 |
| 0x02 | CMD_START_SAMPLE | 无 | 启动采样 |
| 0x03 | CMD_STOP_SAMPLE | 无 | 停止采样 |
| 0x04 | CMD_SET_RATE | 4字节采样率 | 设置采样率 |
| 0x05 | CMD_GET_DATA | 无 | 获取采样数据 |
| 0x06 | CMD_SET_MODE | 1字节模式 | 设置工作模式 |
| 0x07 | CMD_RESET | 无 | 软复位 |
| 0x08 | CMD_OVERCLOCK | 1字节模式 | 超频控制 |

### 1.3 数据类型

| 类型码 | 名称 | 描述 |
|--------|------|------|
| 0x01 | DATA_ANALOG | 模拟采样数据 |
| 0x02 | DATA_DIGITAL | 数字捕获数据 |
| 0x03 | DATA_STATUS | 状态数据 |
| 0xFF | DATA_ERROR | 错误数据 |

### 1.4 模拟数据格式

```
数据结构:
[类型码:1字节] [采样数据:N字节]

每个采样点:
[通道号:4bit] [采样值:12bit] = 2字节
```

### 1.5 数字数据格式

```
数据结构:
[类型码:1字节] [位数据:N字节]

每字节包含8个通道状态:
bit0 = 通道0, bit1 = 通道1, ..., bit7 = 通道7
```

## 2. Pico2 → 树莓派 (USB CDC通信)

### 2.1 帧结构

| 字段 | 长度 | 描述 |
|------|------|------|
| 帧头 | 1字节 | 固定值 0x55 |
| 节点ID | 1字节 | 数据来源节点（0-15，0xFF表示聚合数据） |
| 长度 | 2字节 | 数据长度（小端格式） |
| 数据 | N字节 | 有效数据，最大8192字节 |
| CRC32 | 4字节 | CRC32校验（小端格式） |
| 帧尾 | 1字节 | 固定值 0xAA |

### 2.2 命令码定义

| 命令码 | 名称 | 参数 | 描述 |
|--------|------|------|------|
| 0x01 | CMD_START | 无 | 启动采样 |
| 0x02 | CMD_STOP | 无 | 停止采样 |
| 0x03 | CMD_SET_RATE | 4字节采样率 | 设置采样率 |
| 0x04 | CMD_SET_MODE | 1字节模式 | 设置工作模式 |
| 0x05 | CMD_OVERCLOCK | 1字节模式 | 超频控制 |
| 0x06 | CMD_GET_STATUS | 无 | 获取系统状态 |

### 2.3 超频模式

| 模式 | 频率 | 描述 |
|------|------|------|
| 0 | 默认 | Pico: 133MHz, Pico2: 150MHz |
| 1 | 高性能 | Pico: 200MHz, Pico2: 240MHz |
| 2 | 自动 | 根据温度自动调整 |

## 3. 树莓派 → Web前端 (WebSocket通信)

### 3.1 事件定义

| 事件 | 方向 | 数据 | 描述 |
|------|------|------|------|
| connect | 客户端→服务器 | 无 | 连接建立 |
| disconnect | 客户端→服务器 | 无 | 断开连接 |
| status_update | 服务器→客户端 | 状态对象 | 状态更新 |
| data_update | 服务器→客户端 | 数据对象 | 数据更新 |
| start_sample | 客户端→服务器 | 无 | 启动采样 |
| stop_sample | 客户端→服务器 | 无 | 停止采样 |
| start_crack | 客户端→服务器 | {target_hash, key_length} | 启动破解 |
| stop_crack | 客户端→服务器 | 无 | 停止破解 |
| set_sample_rate | 客户端→服务器 | {rate} | 设置采样率 |
| set_overclock | 客户端→服务器 | {mode} | 设置超频 |

### 3.2 状态对象

```json
{
    "version": "V0.1",
    "work_mode": "IDLE",
    "run_status": "Stop",
    "core_temp": 25,
    "battery": 100,
    "storage": 0,
    "pico_freq": 133,
    "pico2_freq": 150,
    "fault_info": "None"
}
```

### 3.3 数据对象

```json
{
    "analog": [0.0, 0.1, ...],  // 64个模拟通道值
    "digital": [0, 1, ...],     // 128个数字通道状态
    "crack_progress": 0,        // 破解进度
    "crack_result": ""          // 破解结果
}
```

## 4. CRC校验算法

### 4.1 CRC16 (Pico → Pico2)

使用CCITT标准CRC16算法：
- 初始值: 0x0000
- 多项式: 0x1021
- 输入反转: false
- 输出反转: false
- 异或输出: 0x0000

### 4.2 CRC32 (Pico2 → 树莓派)

使用标准CRC32算法（IEEE 802.3）：
- 初始值: 0xFFFFFFFF
- 多项式: 0x04C11DB7
- 输入反转: true
- 输出反转: true
- 异或输出: 0xFFFFFFFF

## 5. 错误码定义

| 错误码 | 名称 | 描述 |
|--------|------|------|
| E01 | PRSNT_LOST | PRSNT信号丢失 |
| E02 | OVERHEAT | 温度过高 |
| E03 | SPI_ERROR | SPI通信错误 |
| E04 | USB_ERROR | USB通信错误 |
| E05 | ADC_ERROR | ADC采样错误 |
| E06 | DIGITAL_ERROR | 数字捕获错误 |
| E07 | STORAGE_ERROR | 存储错误 |
| E08 | FRAM_ERROR | FRAM错误 |

## 6. 时序要求

| 操作 | 最大延迟 | 描述 |
|------|----------|------|
| SPI命令响应 | 10ms | Pico响应Pico2命令 |
| USB数据传输 | 100ms | Pico2向树莓派发送数据 |
| WebSocket更新 | 200ms | 树莓派向Web前端推送数据 |
| 采样周期 | 8μs | 125KSPS采样率 |
| 数字捕获周期 | 10ns | 100MSPS捕获率 |

## 7. 数据流示例

### 7.1 启动采样流程

```
Web前端 → 树莓派: socket.emit('start_sample')
树莓派 → Pico2: USB帧 {cmd: 0x01, params: none}
Pico2 → Pico #0-#15: SPI帧 {cmd: 0x02, params: none}
Pico #0-#15: 开始采样
Pico #0-#15 → Pico2: SPI帧 {data: analog/digital samples}
Pico2: 数据聚合
Pico2 → 树莓派: USB帧 {aggregated data}
树莓派: 数据解析、FFT分析
树莓派 → Web前端: socket.emit('data_update', {analog, digital})
```

### 7.2 破解流程

```
Web前端 → 树莓派: socket.emit('start_crack', {target_hash, key_length})
树莓派: 启动破解引擎
树莓派 → Pico2: USB帧 {cmd: 0x05, params: {mode: 1}} // 超频
Pico2 → Pico #0-#15: SPI帧 {cmd: 0x08, params: {mode: 1}}
树莓派: 暴力破解MD5
树莓派 → Web前端: socket.emit('data_update', {crack_progress, crack_result})
```