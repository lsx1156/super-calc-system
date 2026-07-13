# 硬件设计文档目录

本目录存放超采集算系统的硬件设计文档。

## 目录结构

```
hardware/
├── schematics/          # 原理图
│   ├── rp_main/        # 树莓派主控板
│   ├── pico2_carrier/  # Pico2载波板
│   └── pico_node/      # Pico终端节点板
├── pcb/                 # PCB设计
│   ├── rp_main/
│   ├── pico2_carrier/
│   └── pico_node/
├── bom/                 # 物料清单
│   ├── rp_main_bom.xlsx
│   ├── pico2_carrier_bom.xlsx
│   └── pico_node_bom.xlsx
├── 3d_models/           # 3D模型
└── docs/                # 硬件设计说明
    ├── hardware_spec.md    # 硬件规格书
    ├── power_design.md     # 电源设计
    ├── signal_integrity.md # 信号完整性
    └── mechanical.md       # 机械结构
```

## 硬件规格

### 树莓派主控板
- 主控：Raspberry Pi 4B / 5
- 通信：2× USB CDC (Pico2)
- 显示：1.3" OLED (SSD1306)
- 输入：5向按键 + 3个功能键
- 存储：eMMC + microSD
- 电源：7.4V锂电池 + 充电管理
- 保护：硬件看门狗 + 掉电保护

### Pico2载波板
- 主控：RP2350 (Pico 2)
- 接口：最多16× SPI (Pico终端)
- 通信：USB CDC (100Mbps)
- 电源：5V/2A
- 保护：过流保护 + ESD保护

### Pico终端节点板
- 主控：RP2040 (Pico)
- 模拟：4通道 12位 ADC (125KSPS)
- 数字：8通道 高速GPIO (100MSPS)
- 通信：SPI (20Mbps)
- 电源：3.3V/500mA
- 保护：ESD + 过压保护

## 系统配置

### 完整版配置 (SB)
- 树莓派主控：1台
- Pico2协处理器：最多8台
- Pico终端节点：最多128台 (每Pico2带16台)
- 模拟通道：最多512路
- 数字通道：最多1024路

## 接口定义

### Pico2 ↔ Pico SPI接口
- SCK: GPIO18
- MOSI: GPIO19
- MISO: GPIO16
- CS: GPIO17 (片选)
- INT: GPIO20 (中断)

### Pico2 ↔ 树莓派 USB接口
- USB CDC 虚拟串口
- 波特率：100Mbps
- 端点：BULK IN/OUT
