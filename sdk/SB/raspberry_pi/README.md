# 超采集算系统 - 树莓派主控层

Super Collection Compute System - Raspberry Pi Control Layer

## 概述

本目录包含超采集算系统的树莓派主控层代码，负责与 Pico2 协处理器通信、集群管理、算法处理和 Web 监控。

## 目录结构

```
raspberry_pi/
├── main.py               # 系统主入口
├── requirements.txt      # Python依赖
├── core/                 # 核心模块
│   ├── config.py         # 系统配置
│   ├── logger.py         # 日志系统
│   ├── status_manager.py # 状态管理
│   └── usb_comm.py       # USB通信
├── cluster/              # 集群管理
│   └── adaptive_cluster.py  # 自适应集群
├── modes/                # 模式调度
│   └── mode_scheduler.py # 多模式调度器
├── algorithms/           # 算法库
│   ├── __init__.py
│   ├── analog_algorithms.py       # 模拟信号处理算法
│   ├── digital_algorithms.py      # 数字信号处理算法
│   ├── crack_algorithms.py        # 密码破解算法
│   ├── optimized_algorithms.py    # 向量化优化算法+自动识别
│   └── parallel_processing.py     # 并行处理框架
├── daemons/              # 守护进程
│   ├── watchdog_daemon.py    # 看门狗守护
│   ├── data_receiver.py      # 数据接收
│   ├── business_logic.py     # 业务逻辑
│   ├── storage_remote.py     # 存储与远程
│   ├── mode_scheduler.py     # 模式调度守护
│   ├── hmi_task.py           # 人机交互
│   └── crack_engine_daemon.py # 破解引擎守护
├── services/             # 服务
│   └── web_service.py    # Web监控服务
└── web/                  # Web前端
    ├── templates/        # HTML模板
    └── static/           # 静态资源
        ├── css/
        └── js/
```

## 快速开始

### 环境要求

- 树莓派 4B/5 (或其他Linux主机)
- Python 3.8+

### 安装依赖

```bash
cd raspberry_pi
pip install -r requirements.txt
```

### 启动系统

**看门狗模式（推荐生产环境）：**
```bash
python main.py --mode watchdog
```

**单进程调试模式：**
```bash
python main.py --mode standalone --debug
```

### 访问Web界面

启动后访问：`http://<树莓派IP>:5000`

## USB 通信协议

### USB CDC 帧格式

#### 上行帧（Pico2 → 树莓派）

| 偏移 | 长度 | 字段 | 说明 |
|------|------|------|------|
| 0 | 1 | HEADER | 帧头 0x55 |
| 1 | 1 | NODE_ID | Pico2节点ID |
| 2 | 2 | DATA_LEN | 数据长度（小端） |
| 4 | 4 | TIMESTAMP | 时间戳（微秒） |
| 8 | n | DATA | 数据内容 |
| 8+n | 4 | CRC32 | 校验和（覆盖TIMESTAMP+DATA） |
| 12+n | 1 | TAIL | 帧尾 0xAA |

#### 命令帧（树莓派 → Pico2）

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

### 数据类型

| 类型码 | 名称 | 说明 |
|--------|------|------|
| 0x01 | DATA_ANALOG | 模拟采样数据 |
| 0x02 | DATA_DIGITAL | 数字捕获数据 |
| 0x03 | DATA_STATUS | 状态信息 |
| 0x04 | DATA_CRACK | 破解数据 |
| 0x10 | DATA_AGGREGATED | 聚合数据 |
| 0x11 | DATA_CLUSTER_INFO | 集群信息 |
| 0x12 | DATA_FAULT | 故障信息 |

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

## 算法引擎

### 向量化优化（性能提升）

| 算法 | 原始速度 | 向量化速度 | 加速比 |
|------|---------|-----------|--------|
| 滑动平均(8阶) | ~2.2M samples/sec | ~43.9M samples/sec | 20x |
| FIR滤波(31阶) | ~175K samples/sec | ~33.5M samples/sec | 258x |

### 自动识别功能

- **模拟信号识别**：直流、正弦波、方波、三角波、噪声、脉冲
- **哈希类型识别**：MD5、SHA-1、SHA-256、SHA-512等
- **协议自动识别**：UART、SPI、I2C、GPIO

## 许可证

MIT License - 详见 [LICENSE](../../LICENSE)
