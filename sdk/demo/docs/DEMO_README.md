# 超采集算系统 Demo V0.1 说明文档

## 一、Demo概述

### 1.1 设计目标

本Demo用于**单组验证**超采集算系统的核心功能，在全规模集群搭建之前，先验证关键技术路径：

- ✅ **信号采集分析** — 模拟采样 + 数字捕获 + FFT频谱分析
- ✅ **协议破解** — 侧信道数据聚合 + MD5暴力破解引擎
- ✅ **硬件安全测试** — 电压监控、温度测试、超频稳定性、电压毛刺

### 1.2 Demo规模

| 层级 | 芯片 | 数量 | 职责 |
|------|------|------|------|
| 主控 | 树莓派4B | 1 | Web服务、数据处理、破解引擎 |
| 协处理器 | RP2350 (Pico2) | 1片 | SPI主机、数据聚合、USB通信 |
| 采集节点 | RP2040 (Pico) | 8片 | ADC采样、GPIO捕获、SPI从机 |

**Demo指标：**
- 模拟通道：32路（8片 × 4路）
- 数字通道：64路（8片 × 8路）
- 采样率：最高125KSPS
- SPI总线：20Mbps
- 超频：Pico 200MHz / Pico2 240MHz

---

## 二、目录结构

```
demo/
├── pico/                      # Pico(RP2040) Demo固件
│   └── firmware/
│       ├── main.c            # 主程序（8片通用）
│       └── CMakeLists.txt
├── pico2/                     # Pico2(RP2350) Demo固件
│   └── firmware/
│       ├── main.c            # 协处理器主程序
│       └── CMakeLists.txt
├── raspberry_pi/              # 树莓派Demo主控
│   ├── app_demo.py           # 主程序（单路USB + Web）
│   ├── config_demo.py        # Demo配置
│   ├── templates/
│   │   └── index.html        # Demo Web界面
│   └── static/
│       ├── css/style_demo.css
│       └── js/demo.js
├── tools/                     # 工具脚本
│   ├── demo_deploy.py        # 部署工具（交互式）
│   └── verify_demo.py        # 功能验证测试
└── docs/
    └── DEMO_README.md         # 本文件
```

---

## 三、快速开始

### 3.1 环境要求

**固件编译环境：**
- Pico SDK（设置 `PICO_SDK_PATH` 环境变量）
- CMake ≥ 3.13
- ARM GCC 工具链
- Python 3.8+

**树莓派运行环境：**
- Raspberry Pi OS (Bookworm)
- Python 3.9+
- USB CDC 驱动支持

### 3.2 一键检查环境

```bash
cd demo/tools
python demo_deploy.py
# 选择 1 - 环境完整性检查
```

### 3.3 安装Python依赖

```bash
# 方式1：使用部署工具
python demo_deploy.py
# 选择 5 - 安装Python依赖

# 方式2：手动安装
pip install pyserial flask flask-socketio numpy
```

---

## 四、固件烧录指南

### 4.1 构建Pico固件

```bash
cd demo/pico/firmware
mkdir build && cd build
cmake .. -DPICO_BOARD=pico
make -j4
```

生成文件：`super_calc_demo_pico.uf2`

### 4.2 构建Pico2固件

```bash
cd demo/pico2/firmware
mkdir build && cd build
cmake .. -DPICO_BOARD=pico2
make -j4
```

生成文件：`super_calc_demo_pico2.uf2`

### 4.3 烧录步骤

1. 按住Pico的BOOTSEL按钮，插入USB
2. 电脑出现U盘盘符
3. 将 `.uf2` 文件复制到U盘
4. Pico自动重启，固件开始运行

**烧录8片Pico：** 每片都烧录同一个固件，通过SPI CS引脚区分节点。

---

## 五、硬件连接

### 5.1 SPI连接（Pico2 ↔ 8×Pico）

| Pico2引脚 | 功能 | 连接到 |
|-----------|------|--------|
| GPIO10 | SCK | 所有Pico的GPIO2 (SCK) |
| GPIO11 | MOSI | 所有Pico的GPIO3 (MOSI) |
| GPIO12 | MISO | 所有Pico的GPIO4 (MISO) |
| GPIO2 | CS0 | Pico #0 的GPIO1 (CS) |
| GPIO3 | CS1 | Pico #1 的GPIO1 (CS) |
| ... | ... | ... |
| GPIO9 | CS7 | Pico #7 的GPIO1 (CS) |
| GND | GND | 公共地 |

### 5.2 Pico模拟输入

每片Pico的GPIO26-29（4路ADC）作为模拟输入。

### 5.3 Pico数字输入

每片Pico的GPIO5-GPIO12（8路）作为数字输入。

### 5.4 USB连接

Pico2通过USB连接树莓派，USB CDC虚拟串口通信。

---

## 六、启动Web界面

### 6.1 在树莓派上运行

```bash
cd demo/raspberry_pi
python app_demo.py
```

### 6.2 访问界面

在浏览器打开：`http://<树莓派IP>:5000`

---

## 七、三大核心功能说明

### 7.1 信号采集分析

**功能：**
- 32路模拟实时采样（12bit ADC）
- 64路数字通道状态显示
- 实时FFT频谱分析
- 主频率、信噪比计算

**操作步骤：**
1. 切换到「信号采集分析」选项卡
2. 选择采样率（10K/50K/100K/125K SPS）
3. 选择超频模式（可选）
4. 点击「开始采样」
5. 观察波形图和频谱图

**验证标准：**
- 波形随输入信号变化
- FFT频谱能正确反映主频
- 8片Pico状态显示在线

### 7.2 协议破解

**功能：**
- MD5暴力破解引擎
- 可配置密钥长度（4-24位）
- 可配置字符集
- 实时进度、速度、预估时间

**操作步骤：**
1. 切换到「协议破解」选项卡
2. 输入目标MD5哈希
3. 设置密钥长度和字符集
4. 点击「开始破解」
5. 观察进度和结果

**Demo测试用例：**
- 空字符串MD5：`d41d8cd98f00b204e9800998ecf8427e`
- 4位数字测试：先用 `1234` 的MD5试跑

### 7.3 硬件安全测试

**测试项目：**

| 测试项 | 说明 | 时长 |
|--------|------|------|
| 电压监控 | 核心电压测量 | 即时 |
| 时钟频率 | 系统时钟验证 | 即时 |
| 温度测试 | 30秒连续监测 | 30秒 |
| 超频稳定性 | 超频下10秒负载 | 10秒 |
| 电压毛刺注入 | 10次递增脉宽毛刺 | 5秒 |

**操作步骤：**
1. 切换到「硬件安全测试」选项卡
2. 选择左侧测试项目
3. 点击「运行」按钮
4. 查看测试结果

**注意：** 电压毛刺测试需要外部硬件电路支持。

---

## 八、验证清单

### 8.1 硬件验证

- [ ] Pico2通电，USB枚举成功
- [ ] 8片Pico全部通电
- [ ] SPI连接正确（SCK/MOSI/MISO/CS）
- [ ] 模拟输入信号源连接
- [ ] 数字输入信号源连接
- [ ] 公共地连接

### 8.2 固件验证

- [ ] Pico固件编译通过
- [ ] Pico2固件编译通过
- [ ] 8片Pico固件烧录成功
- [ ] Pico2固件烧录成功
- [ ] Pico2 USB CDC枚举成功

### 8.3 功能验证

**信号采集：**
- [ ] Web界面正常打开
- [ ] 8片Pico检测到（显示在线）
- [ ] 开始采样后波形有变化
- [ ] 模拟电压值合理（0-3.3V）
- [ ] 数字通道状态正确
- [ ] FFT频谱正常显示
- [ ] 采样率设置生效

**协议破解：**
- [ ] 破解任务可启动
- [ ] 进度条正常更新
- [ ] 尝试次数递增
- [ ] 速度计算正确
- [ ] 已知测试用例可破解成功
- [ ] 停止功能正常

**硬件安全测试：**
- [ ] 电压监控返回数据
- [ ] 时钟频率测试正常
- [ ] 温度测试数据曲线正常
- [ ] 超频切换成功
- [ ] 超频后系统稳定
- [ ] 电压毛刺指令下发成功
- [ ] 测试结果正确显示

### 8.4 性能验证

- [ ] 50KSPS采样不丢数
- [ ] SPI通信CRC校验通过率 > 99%
- [ ] Web界面延迟 < 200ms
- [ ] 超频模式下温度 < 70°C
- [ ] 连续运行30分钟无死机

---

## 九、常见问题

### Q1: Pico检测不到？
A: 检查SPI连线、CS引脚、Pico固件是否正确烧录。

### Q2: USB串口找不到？
A: 确认Pico2固件已烧录，USB线支持数据传输，安装USB CDC驱动。

### Q3: 波形显示异常？
A: 检查模拟输入信号幅度（0-3.3V），确认ADC引脚连接正确。

### Q4: 破解速度慢？
A: Demo版破解引擎为纯CPU实现，主要用于验证流程。全集群版将分布式计算。

### Q5: 超频后不稳定？
A: 降低超频频率，确保散热良好，核心电压可适当提升（注意风险）。

---

## 十、后续扩展

Demo验证通过后，可向全规模扩展：

| 维度 | Demo | 全规模 |
|------|------|--------|
| Pico数量 | 8片 | 64片 |
| Pico2数量 | 1片 | 8片 |
| 模拟通道 | 32路 | 256路 |
| 数字通道 | 64路 | 512路 |
| 破解算力 | 单线程 | 分布式 |
| 树莓派 | 1台 | 1台（管理8个Pico2） |

---

## 附录：命令速查

```bash
# 环境检查
python demo/tools/demo_deploy.py

# 功能测试
python demo/tools/verify_demo.py

# 构建Pico固件
cd demo/pico/firmware && mkdir build && cd build
cmake .. -DPICO_BOARD=pico && make -j4

# 构建Pico2固件
cd demo/pico2/firmware && mkdir build && cd build
cmake .. -DPICO_BOARD=pico2 && make -j4

# 启动Web服务
python demo/raspberry_pi/app_demo.py
```

---

*文档版本: V0.1 Demo*  
*最后更新: 2025年*