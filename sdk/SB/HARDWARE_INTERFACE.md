# HARDWARE_INTERFACE.md - 完整版硬件接口契约书

## 文档版本
| 属性 | 值 |
|------|-----|
| 版本 | 1.2 |
| 日期 | 2026-06-30 |
| 适用版本 | 完整版（Zero 2W适配） |
| 开发环境 | Arduino IDE 2.x + Arduino-Pico核心库 |
| 主控平台 | 树莓派Zero 2W（512MB RAM） |

---

## 1. 硬件架构概述

### 1.1 系统拓扑（Zero 2W版）
```
┌──────────────┐     USB CDC (100Mbps)     ┌──────────────┐
│  树莓派Zero 2W │◄────────────────────────►│   Pico2#0    │
│   主控节点    │                          │   RP2350     │
│  512MB RAM   │                          └──────┬───────┘
└──────────────┘                                  │
                              ┌───────────────────┼───────────────────┐
                              │                   │                   │
                              ▼                   ▼                   ▼
                         ┌─────────┐       ┌─────────┐       ┌─────────┐
                         │  Pico   │       │  Pico   │       │  Pico   │
                         │ #0-#7   │       │ 扩展槽位 │       │ 扩展槽位 │
                         │  RP2040 │       │  RP2040 │       │  RP2040 │
                         └─────────┘       └─────────┘       └─────────┘
                              │                   │                   │
                         4路ADC + 8路数字    4路ADC + 8路数字    4路ADC + 8路数字
                              │                   │                   │
                              └───────────────────┴───────────────────┘
                                             共8片Pico（Zero 2W限制）
```

### 1.2 通信协议
| 链路 | 协议 | 速率 | 方向 |
|------|------|------|------|
| 树莓派Zero 2W ↔ Pico2 | USB CDC | 100Mbps | 双向 |
| Pico2 ↔ Pico | SPI | 20Mbps | 双向（数据） |
| Pico2 ↔ Pico | I2C | 400kHz | 双向（控制） |

### 1.3 Zero 2W硬件限制
| 参数 | Zero 2W规格 | 影响 |
|------|------------|------|
| RAM | 512MB | 限制节点数量和队列大小 |
| USB端口 | 1个 | 仅支持1个Pico2 |
| CPU | 单核（4线程） | 减少并发线程数 |
| 看门狗 | 无硬件看门狗 | 使用软件看门狗 |
| 散热 | 较差 | 降低温度阈值和超频时间 |

---

## 2. 开发环境配置

### 2.1 Arduino IDE设置
| 芯片 | 开发板选择 | 核心库版本 | 注意事项 |
|------|-----------|-----------|----------|
| RP2040 (Pico) | Raspberry Pi Pico | Arduino-Pico 3.x | 无FreeRTOS |
| RP2350 (Pico2) | Raspberry Pi Pico2 | Arduino-Pico 4.x | 支持双核 |

### 2.2 Arduino-Pico时钟函数映射
| Pico SDK函数 | Arduino-Pico函数 | 说明 |
|--------------|-------------------|------|
| `set_sys_clock_khz(freq, true)` | `setSystemClock(freq/1000)` | 设置系统时钟 |
| `vreg_set_voltage(vreg_voltage)` | 无直接对应 | 使用Flash频率间接调整 |
| `clock_get_hz(clk_sys)` | `F_CPU` 或 `getSystemClock()` | 获取当前时钟 |

### 2.3 Arduino-Pico电压调整方案
```cpp
// Arduino IDE下的超频方案（RP2040）
// 200MHz需要Flash频率设置为64MHz
#include <hardware/clocks.h>

void overclock_enable() {
    // 设置Flash时钟频率（必须在setSystemClock之前）
    rp2040.setFlashFrequency(64);  // 64MHz Flash频率支持200MHz系统时钟
    setSystemClock(200);           // 设置系统时钟为200MHz
}

void overclock_disable() {
    setSystemClock(133);           // 回到默认133MHz
    rp2040.setFlashFrequency(50);  // 默认Flash频率
}
```

### 2.4 RP2350超频方案
```cpp
// Arduino IDE下的超频方案（RP2350）
void overclock_enable_pico2() {
    rp2350.setFlashFrequency(80);  // 80MHz Flash频率支持240MHz系统时钟
    setSystemClock(240);           // 设置系统时钟为240MHz
}

void overclock_disable_pico2() {
    setSystemClock(150);           // 回到默认150MHz
    rp2350.setFlashFrequency(50);  // 默认Flash频率
}
```

---

## 3. 频率与模式关联规范

### 3.1 工作模式频率配置表
| 模式 | 编号 | RP2040频率 | RP2350频率 | 超频需求 | 说明 |
|------|------|-----------|-----------|----------|------|
| MODE_SAMPLE | 0x00 | 133MHz | 150MHz | 不需要 | 采样模式，默认频率足够 |
| MODE_CRACK | 0x01 | 133MHz | 150MHz | 可选 | 破译模式，侧信道数据采集 |
| MODE_BRUTEFORCE | 0x02 | 200MHz | 240MHz | **必须** | 暴力破解，需要超频加速计算 |
| MODE_HW_TEST | 0x03 | 133MHz → 200MHz | 150MHz → 240MHz | 动态 | 硬件测试，可动态调整频率 |

### 3.2 频率切换时序约束
```
超频流程：
┌───────────────────────────────────────────────────────────────┐
│ 1. 停止SPI/I2C通信（避免通信不稳定）                           │
│    - spi_master_suspend()                                      │
│    - i2c_sched_suspend()                                       │
│                                                                │
│ 2. 停止ADC采样和数字捕获（避免采样率突变）                      │
│    - adc_sample_stop()                                         │
│    - digital_capture_stop()                                    │
│                                                                │
│ 3. 设置Flash频率（必须先设置）                                 │
│    - rp2040.setFlashFrequency(64) // RP2040                   │
│    - 或 rp2350.setFlashFrequency(80) // RP2350                │
│                                                                │
│ 4. 设置系统时钟                                                │
│    - setSystemClock(200) // RP2040超频                        │
│    - 或 setSystemClock(240) // RP2350超频                     │
│                                                                │
│ 5. 等待时钟稳定                                                │
│    - delay(10)                                                 │
│                                                                │
│ 6. 重新计算通信参数                                            │
│    - SPI波特率 = F_CPU / 分频系数                              │
│    - ADC采样率分频调整                                         │
│                                                                │
│ 7. 恢复通信和采样                                              │
│    - spi_master_resume()                                       │
│    - i2c_sched_resume()                                        │
│    - adc_sample_start()                                        │
│    - digital_capture_start()                                   │
└───────────────────────────────────────────────────────────────┘
```

### 3.3 频率与采样率关联表
| 系统时钟 | ADC最大采样率 | SPI波特率 | I2C频率 | 说明 |
|----------|--------------|----------|---------|------|
| 133MHz (RP2040默认) | 125KSPS | 20Mbps | 400kHz | 正常工作 |
| 200MHz (RP2040超频) | 187KSPS | 30Mbps | 600kHz | **超出ADC规格** |
| 150MHz (RP2350默认) | 125KSPS | 20Mbps | 400kHz | 正常工作 |
| 240MHz (RP2350超频) | 200KSPS | 32Mbps | 640kHz | **超出ADC规格** |

### 3.4 ⚠️ 硬件冲突警告
| 冲突项 | 问题描述 | 解决方案 |
|--------|----------|----------|
| **ADC超频冲突** | RP2040超频后ADC采样率超出芯片规格（187KSPS > 125KSPS最大值） | **强制限制采样率上限**：超频时ADC采样率设置为100KSPS |
| **SPI波特率变化** | 超频后SPI波特率会自动改变，导致通信不稳定 | 使用`spi_set_baudrate()`重新设置固定波特率 |
| **I2C频率变化** | 超频后I2C频率可能超出400kHz快速模式规格 | 使用`i2c_set_baudrate()`重新设置400kHz |
| **温度升高** | 超频后芯片温度可能达到70°C触发过热保护 | 仅在MODE_BRUTEFORCE模式使用超频，持续时间限制 |

---

## 4. Pico终端芯片（RP2040）引脚分配

### 4.1 引脚占用表（无频率冲突）

| GPIO编号 | 功能 | 方向 | 硬件说明 | 频率相关约束 |
|----------|------|------|----------|--------------|
| 0 | I2C SDA | IN/OUT | I2C调度总线数据 | 超频后需重新初始化I2C |
| 1 | SPI CS | IN | Pico2片选信号 | 无频率影响 |
| 2 | SPI SCK | IN | SPI时钟输入 | 无频率影响（由主机提供） |
| 3 | SPI MOSI | IN | 从Pico2接收数据 | 无频率影响 |
| 4 | SPI MISO | OUT | 向Pico2发送数据 | 无频率影响 |
| 5 | I2C SCL | IN | I2C调度总线时钟 | 超频后需重新初始化I2C |
| 6 | 数字输入通道0 | IN | 数字捕获通道0 | PIO时钟分频需调整 |
| 7 | 数字输入通道1 | IN | 数字捕获通道1 | PIO时钟分频需调整 |
| 8 | 数字输入通道2 | IN | 数字捕获通道2 | PIO时钟分频需调整 |
| 9 | 数字输入通道3 | IN | 数字捕获通道3 | PIO时钟分频需调整 |
| 10 | 数字输入通道4 | IN | 数字捕获通道4 | PIO时钟分频需调整 |
| 11 | 数字输入通道5 | IN | 数字捕获通道5 | PIO时钟分频需调整 |
| 12 | 数字输入通道6 | IN | 数字捕获通道6 | PIO时钟分频需调整 |
| 13 | 数字输入通道7 | IN | 数字捕获通道7 | PIO时钟分频需调整 |
| 26 | ADC通道0 | IN | 模拟输入通道0 | **超频时采样率限制为100KSPS** |
| 27 | ADC通道1 | IN | 模拟输入通道1 | **超频时采样率限制为100KSPS** |
| 28 | ADC通道2 | IN | 模拟输入通道2 | **超频时采样率限制为100KSPS** |
| 29 | ADC通道3 | IN | 模拟输入通道3 | **超频时采样率限制为100KSPS** |
| 25 | LED指示 | OUT | 运行状态指示灯 | 无频率影响 |

### 4.2 功能区块

#### SPI从机接口
| 引脚 | 信号 | SPI端口 | 频率约束 |
|------|------|---------|----------|
| GPIO1 | CS | spi0 | 无频率依赖 |
| GPIO2 | SCK | spi0 | 主机提供时钟，无影响 |
| GPIO3 | MOSI | spi0 | 无频率依赖 |
| GPIO4 | MISO | spi0 | 无频率依赖 |

#### I2C从机接口
| 引脚 | 信号 | I2C端口 | 频率约束 |
|------|------|---------|----------|
| GPIO0 | SDA | i2c0 | **超频后需重新初始化，保持400kHz** |
| GPIO5 | SCL | i2c0 | **超频后需重新初始化，保持400kHz** |

#### ADC模拟采样
| 引脚 | ADC通道 | 说明 | 频率约束 |
|------|---------|------|----------|
| GPIO26 | ADC0 | 模拟输入0 | **超频时采样率上限100KSPS** |
| GPIO27 | ADC1 | 模拟输入1 | **超频时采样率上限100KSPS** |
| GPIO28 | ADC2 | 模拟输入2 | **超频时采样率上限100KSPS** |
| GPIO29 | ADC3 | 模拟输入3 | **超频时采样率上限100KSPS** |

#### 数字捕获（PIO）
| 引脚 | 数字通道 | PIO时钟约束 |
|------|---------|-------------|
| GPIO6 | D0 | PIO分频需根据系统时钟调整 |
| GPIO7 | D1 | PIO分频需根据系统时钟调整 |
| GPIO8 | D2 | PIO分频需根据系统时钟调整 |
| GPIO9 | D3 | PIO分频需根据系统时钟调整 |
| GPIO10 | D4 | PIO分频需根据系统时钟调整 |
| GPIO11 | D5 | PIO分频需根据系统时钟调整 |
| GPIO12 | D6 | PIO分频需根据系统时钟调整 |
| GPIO13 | D7 | PIO分频需根据系统时钟调整 |

---

## 5. Pico2协处理器（RP2350）引脚分配

### 5.1 引脚占用表

| GPIO编号 | 功能 | 方向 | 硬件说明 | 频率相关约束 |
|----------|------|------|----------|--------------|
| 2 | SPI CS0 | OUT | Pico#0片选 | 无频率影响 |
| 3 | SPI CS1 | OUT | Pico#1片选 | 无频率影响 |
| 4 | SPI CS2 | OUT | Pico#2片选 | 无频率影响 |
| 5 | SPI CS3 | OUT | Pico#3片选 | 无频率影响 |
| 6 | SPI CS4 | OUT | Pico#4片选 | 无频率影响 |
| 7 | SPI CS5 | OUT | Pico#5片选 | 无频率影响 |
| 8 | SPI CS6 | OUT | Pico#6片选 | 无频率影响 |
| 9 | SPI CS7 | OUT | Pico#7片选 | 无频率影响 |
| 10 | SPI SCK | OUT | SPI时钟输出 | **超频后需重新设置波特率保持20Mbps** |
| 11 | SPI MOSI | OUT | SPI数据输出 | **超频后需重新设置波特率保持20Mbps** |
| 12 | SPI MISO | IN | SPI数据输入 | **超频后需重新设置波特率保持20Mbps** |
| 13 | SPI CS8 | OUT | Pico#8片选 | 无频率影响 |
| 14 | SPI CS9 | OUT | Pico#9片选 | 无频率影响 |
| 15 | SPI CS10 | OUT | Pico#10片选 | 无频率影响 |
| 16 | SPI CS11 | OUT | Pico#11片选 | 无频率影响 |
| 17 | SPI CS12 | OUT | Pico#12片选 | 无频率影响 |
| 18 | SPI CS13 | OUT | Pico#13片选 | 无频率影响 |
| 19 | SPI CS14 | OUT | Pico#14片选 | 无频率影响 |
| 20 | SPI CS15 | OUT | Pico#15片选 | 无频率影响 |
| 26 | I2C SDA | IN/OUT | I2C调度总线数据 | **超频后需重新设置频率保持400kHz** |
| 27 | I2C SCL | OUT | I2C调度总线时钟 | **超频后需重新设置频率保持400kHz** |
| 25 | LED指示 | OUT | 运行状态指示灯 | 无频率影响 |

### 5.2 功能区块

#### SPI主机接口
| 引脚 | 信号 | SPI端口 | 频率约束 |
|------|------|---------|----------|
| GPIO10 | SCK | spi1 | **超频后需调用spi_set_baudrate(20000000)** |
| GPIO11 | MOSI | spi1 | **超频后需调用spi_set_baudrate(20000000)** |
| GPIO12 | MISO | spi1 | **超频后需调用spi_set_baudrate(20000000)** |
| GPIO2-20 | CS0-CS15 | - | 无频率依赖 |

#### I2C主机接口
| 引脚 | 信号 | I2C端口 | 频率约束 |
|------|------|---------|----------|
| GPIO26 | SDA | i2c0 | **超频后需调用i2c_set_baudrate(400000)** |
| GPIO27 | SCL | i2c0 | **超频后需调用i2c_set_baudrate(400000)** |

#### USB CDC接口
- 使用RP2350内置USB外设，无需额外GPIO
- USB通信不受系统时钟影响

---

## 6. 电气规范

### 6.1 电压等级
| 信号类型 | 电压 | 说明 | 超频影响 |
|----------|------|------|----------|
| 模拟输入 | 0-3.3V | ADC参考电压3.3V | 无变化 |
| 数字输入 | 0-3.3V | CMOS电平 | 无变化 |
| 数字输出 | 0-3.3V | CMOS电平 | 无变化 |
| SPI信号 | 3.3V | LVCMOS | 无变化 |
| I2C信号 | 3.3V | 带上拉（4.7kΩ） | 无变化 |
| **核心电压（超频）** | 1.20V | 超频时需要 | Arduino IDE通过Flash频率间接调整 |

### 6.2 时序参数
| 参数 | 默认值 | 超频值 | 说明 |
|------|--------|--------|------|
| SPI时钟 | 20MHz | 20MHz（固定） | 超频后需重新设置保持不变 |
| I2C时钟 | 400kHz | 400kHz（固定） | 超频后需重新设置保持不变 |
| ADC采样率 | 125KSPS | **100KSPS（限制）** | 超频时强制限制以保护ADC |
| 数字捕获率 | 50MHz | 50MHz | PIO分频调整保持不变 |
| RP2040系统时钟 | 133MHz | 200MHz | MODE_BRUTEFORCE专用 |
| RP2350系统时钟 | 150MHz | 240MHz | MODE_BRUTEFORCE专用 |

---

## 7. 超频安全约束

### 7.1 温度限制
| 温度阈值 | 动作 | 说明 |
|----------|------|------|
| < 60°C | 允许超频 | 正常工作 |
| 60-70°C | 警告 | 监控温度变化 |
| ≥ 70°C | **强制降频** | 立即恢复默认频率，停止超频 |

### 7.2 超频持续时间限制
| 模式 | 最大超频时间 | 原因 |
|------|--------------|------|
| MODE_BRUTEFORCE | 30分钟 | 破解任务完成后自动降频 |
| MODE_HW_TEST | 5分钟 | 测试完成后立即降频 |
| 其他模式 | 不允许超频 | 保护ADC和通信稳定 |

### 7.3 超频前检查清单
```cpp
// Arduino IDE下的超频前检查
bool can_overclock() {
    // 1. 检查温度
    float temp = read_internal_temp();
    if (temp >= 60.0) return false;
    
    // 2. 检查当前模式
    if (current_mode != MODE_BRUTEFORCE && current_mode != MODE_HW_TEST) {
        return false;
    }
    
    // 3. 检查通信状态
    if (spi_is_busy() || i2c_is_busy()) return false;
    
    // 4. 检查ADC采样状态
    if (adc_is_sampling()) return false;
    
    return true;
}
```

---

## 8. Arduino IDE代码适配示例

### 8.1 Pico (RP2040) 固件框架
```cpp
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

// 引脚定义（对应契约表）
#define SPI_CS_PIN    1
#define SPI_SCK_PIN   2
#define SPI_MOSI_PIN  3
#define SPI_MISO_PIN  4
#define I2C_SDA_PIN   0
#define I2C_SCL_PIN   5
#define DIGITAL_START_PIN 6
#define ADC_START_PIN      26

// 模式定义
#define MODE_SAMPLE      0x00
#define MODE_CRACK       0x01
#define MODE_BRUTEFORCE  0x02
#define MODE_HW_TEST     0x03

uint8_t current_mode = MODE_SAMPLE;
uint32_t sample_rate = 50000;

void setup() {
    // 初始化SPI从机
    SPI.setRX(SPI_MISO_PIN);
    SPI.setTX(SPI_MOSI_PIN);
    SPI.setSCK(SPI_SCK_PIN);
    SPI.setCS(SPI_CS_PIN);
    SPI.begin(false);  // 从机模式
    
    // 初始化I2C从机
    Wire.setSDA(I2C_SDA_PIN);
    Wire.setSCL(I2C_SCL_PIN);
    Wire.begin(0xFF);  // 未分配地址
    
    // 初始化数字输入
    for (int i = 0; i < 8; i++) {
        pinMode(DIGITAL_START_PIN + i, INPUT_PULLDOWN);
    }
    
    // 初始化ADC
    analogReadResolution(12);
    for (int i = 0; i < 4; i++) {
        pinMode(ADC_START_PIN + i, INPUT);
    }
}

void set_mode(uint8_t mode) {
    current_mode = mode;
    
    if (mode == MODE_BRUTEFORCE) {
        // 超频前停止所有通信和采样
        SPI.end();
        Wire.end();
        
        // 设置Flash频率和系统时钟
        rp2040.setFlashFrequency(64);  // 64MHz Flash
        setSystemClock(200);           // 200MHz系统时钟
        
        delay(10);  // 等待稳定
        
        // 重新初始化通信（固定波特率）
        SPI.setRX(SPI_MISO_PIN);
        SPI.setTX(SPI_MOSI_PIN);
        SPI.setSCK(SPI_SCK_PIN);
        SPI.setCS(SPI_CS_PIN);
        SPI.begin(false);
        
        Wire.setSDA(I2C_SDA_PIN);
        Wire.setSCL(I2C_SCL_PIN);
        Wire.begin(0xFF);
        
        // 限制采样率
        sample_rate = 100000;  // 超频时限制为100KSPS
    } else {
        // 恢复默认频率
        if (rp2040.getClockFrequency() != 133000000) {
            SPI.end();
            Wire.end();
            
            setSystemClock(133);
            rp2040.setFlashFrequency(50);
            
            delay(10);
            
            SPI.begin(false);
            Wire.begin(0xFF);
            
            sample_rate = 50000;  // 恢复默认采样率
        }
    }
}

void loop() {
    // 处理SPI命令
    if (SPI.available()) {
        uint8_t cmd = SPI.read();
        handle_command(cmd);
    }
    
    // 处理采样
    if (current_mode == MODE_SAMPLE || current_mode == MODE_CRACK) {
        sample_and_send();
    }
    
    // 温度监控
    float temp = rp2040.getInternalTemperature();
    if (temp >= 70.0 && rp2040.getClockFrequency() > 133000000) {
        set_mode(MODE_SAMPLE);  // 过热降频
    }
}
```

### 8.2 Pico2 (RP2350) 固件框架
```cpp
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <SerialUSB.h>

#define NUM_PICO_SLAVES 16
#define SPI_CS_BASE     2
#define I2C_SDA_PIN     26
#define I2C_SCL_PIN     27

void setup() {
    // 初始化USB CDC
    Serial.begin(100000000);
    
    // 初始化SPI主机
    SPI.setRX(12);   // MISO
    SPI.setTX(11);   // MOSI
    SPI.setSCK(10);
    for (int i = 0; i < NUM_PICO_SLAVES; i++) {
        pinMode(SPI_CS_BASE + i, OUTPUT);
        digitalWrite(SPI_CS_BASE + i, HIGH);
    }
    SPI.begin();
    SPI.setClockDivider(F_CPU / 20000000);  // 20Mbps
    
    // 初始化I2C主机
    Wire.setSDA(I2C_SDA_PIN);
    Wire.setSCL(I2C_SCL_PIN);
    Wire.begin();
    Wire.setClock(400000);
    
    // 检测Pico节点
    detect_pico_nodes();
}

void overclock_for_bruteforce() {
    // 停止SPI和I2C
    SPI.end();
    Wire.end();
    
    // 设置超频
    rp2350.setFlashFrequency(80);
    setSystemClock(240);
    
    delay(10);
    
    // 重新初始化通信（固定波特率）
    SPI.begin();
    SPI.setClockDivider(240000000 / 20000000);  // 保持20Mbps
    
    Wire.begin();
    Wire.setClock(400000);  // 保持400kHz
    
    // 广播超频命令给所有Pico
    broadcast_overclock(true);
}
```

---

## 9. 通信协议

### 9.1 SPI帧格式（Pico2 ↔ Pico）
```
[FRAME_HEADER] [DATA_LEN] [CMD] [PARAMS...] [CRC16] [FRAME_TAIL]
     1字节        2字节      1字节    N字节      2字节      1字节
```

| 字段 | 值 | 说明 |
|------|-----|------|
| FRAME_HEADER | 0xAA | 帧起始标识 |
| FRAME_TAIL | 0x55 | 帧结束标识 |

### 9.2 I2C寄存器映射
| 寄存器地址 | 名称 | 读写 | 说明 | 频率约束 |
|-----------|------|------|------|----------|
| 0x00 | NODE_ID | RO | 节点ID | 无 |
| 0x01 | CTRL | RW | 控制寄存器 | 无 |
| 0x02 | MODE | RW | 工作模式 | **超频关联** |
| 0x03 | SAMPLE_RATE | RW | 采样率 | **超频时限制100KSPS** |
| 0x04 | STATUS | RO | 状态寄存器 | 无 |
| 0x05 | ERROR | RO | 错误寄存器 | 无 |
| 0x06 | SYNC_TRIGGER | WO | 同步触发 | 无 |
| 0x07 | ADC_CH_SEL | RW | ADC通道选择 | 无 |
| 0x08 | DIG_CH_SEL | RW | 数字通道选择 | 无 |
| 0x09 | CHECKSUM | RO | 校验和 | 无 |
| 0x10 | OVERCLOCK | RW | 超频控制 | **仅MODE_BRUTEFORCE可用** |

### 9.3 USB CDC帧格式（树莓派 ↔ Pico2）
```
[FRAME_HEADER] [NODE_ID] [DATA_LEN] [DATA...] [CRC32] [FRAME_TAIL]
     1字节        1字节      2字节     N字节      4字节      1字节
```

| 字段 | 值 | 说明 |
|------|-----|------|
| FRAME_HEADER | 0x55 | USB帧起始标识 |
| FRAME_TAIL | 0xAA | USB帧结束标识 |

---

## 10. 引脚冲突检测

### 10.1 Pico端冲突检查
| 功能 | 占用引脚 | 冲突检查 | 状态 |
|------|---------|----------|------|
| I2C从机 | GPIO0, GPIO5 | 无重叠 | ✅ 无冲突 |
| SPI从机 | GPIO1-4 | 无重叠 | ✅ 无冲突 |
| 数字捕获 | GPIO6-13 | 无重叠 | ✅ 无冲突 |
| ADC采样 | GPIO26-29 | 无重叠 | ✅ 无冲突 |
| LED | GPIO25 | 无重叠 | ✅ 无冲突 |

### 10.2 Pico2端冲突检查
| 功能 | 占用引脚 | 冲突检查 | 状态 |
|------|---------|----------|------|
| SPI主机 | GPIO10-12 | 无重叠 | ✅ 无冲突 |
| SPI CS | GPIO2-20 | 无重叠 | ✅ 无冲突 |
| I2C主机 | GPIO26-27 | 无重叠 | ✅ 无冲突 |
| LED | GPIO25 | 无重叠 | ✅ 无冲突 |

---

## 11. 地址分配方案

### 11.1 I2C地址映射
| 槽位编号 | I2C地址 | 计算公式 |
|----------|---------|----------|
| 0 | 0x40 | BASE + 0 |
| 1 | 0x41 | BASE + 1 |
| ... | ... | ... |
| 15 | 0x4F | BASE + 15 |

**计算公式**: `I2C_ADDR = 0x40 + SLOT_ID`

### 11.2 地址分配流程
1. Pico上电后I2C地址为0xFF（未分配）
2. Pico2通过SPI片选线选中指定槽位的Pico
3. Pico2发送CMD_ADDR_ASSIGN命令，包含槽位编号
4. Pico计算I2C地址并设置
5. Pico2通过I2C验证地址是否生效

---

## 12. 硬件接口变更记录

| 版本 | 日期 | 变更内容 |
|------|------|----------|
| 1.0 | 2026-06-30 | 初始版本 |
| 1.1 | 2026-06-30 | 添加Arduino IDE开发环境适配、频率与模式关联规范、超频安全约束 |

---

## 13. 冲突修复说明

### 13.1 原设计问题
早期版本中数字捕获使用GPIO0-7，与I2C的GPIO0和GPIO5冲突。

### 13.2 修复方案
将数字捕获通道从GPIO0-7调整为GPIO6-13，避开I2C占用的GPIO0和GPIO5。

| 旧设计 | 新设计 | 说明 |
|--------|--------|------|
| GPIO0 = D0 | GPIO6 = D0 | 避开I2C SDA |
| GPIO5 = D5 | GPIO11 = D5 | 避开I2C SCL |

### 13.3 频率冲突修复
| 问题 | 解决方案 |
|------|----------|
| 超频后ADC采样率超出规格 | 强制限制采样率上限为100KSPS |
| 超频后SPI波特率变化 | 使用spi_set_baudrate()固定为20Mbps |
| 超频后I2C频率变化 | 使用i2c_set_baudrate()固定为400kHz |
| 超频导致温度过高 | 添加温度监控和强制降频机制 |

### 13.4 影响范围
- 硬件设计：需更新PCB布局
- 固件修改：需更新digital_capture.c中的DIGITAL_PIN_START定义
- Arduino IDE：需使用rp2040/rp2350.setFlashFrequency()调整Flash频率