# HARDWARE_INTERFACE.md - Demo版硬件接口契约书

## 文档版本
| 属性 | 值 |
|------|-----|
| 版本 | 1.2 |
| 日期 | 2026-06-30 |
| 适用版本 | Demo验证版（1组RP2350 + 8组RP2040） |
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
                         │ #0      │       │ #1      │       │ #2      │
                         │  RP2040 │       │  RP2040 │       │  RP2040 │
                         └─────────┘       └─────────┘       └─────────┘
                              │                   │                   │
                         4路ADC + 8路数字    4路ADC + 8路数字    4路ADC + 8路数字
                              │                   │                   │
                              └───────────────────┴───────────────────┘
                                             共8片Pico（Demo版）
```

### 1.2 通信协议
| 链路 | 协议 | 速率 | 方向 |
|------|------|------|------|
| 树莓派 ↔ Pico2 | USB CDC | 100Mbps | 双向 |
| Pico2 ↔ Pico | SPI | 20Mbps | 双向 |

**注：Demo版本不使用I2C调度总线，简化硬件连接**

---

## 2. 开发环境配置

### 2.1 Arduino IDE设置
| 芯片 | 开发板选择 | 核心库版本 | 注意事项 |
|------|-----------|-----------|----------|
| RP2040 (Pico) | Raspberry Pi Pico | Arduino-Pico 3.x | 双核任务 |
| RP2350 (Pico2) | Raspberry Pi Pico2 | Arduino-Pico 4.x | 双核任务 |

### 2.2 Arduino-Pico时钟函数映射
| Pico SDK函数 | Arduino-Pico函数 | 说明 |
|--------------|-------------------|------|
| `set_sys_clock_khz(freq, true)` | `setSystemClock(freq/1000)` | 设置系统时钟 |
| `vreg_set_voltage(vreg_voltage)` | 无直接对应 | 使用Flash频率间接调整 |
| `clock_get_hz(clk_sys)` | `F_CPU` 或 `getSystemClock()` | 获取当前时钟 |

---

## 3. 频率与模式关联规范

### 3.1 工作模式频率配置表（Demo版）
| 模式 | 编号 | RP2040频率 | RP2350频率 | 超频需求 | 说明 |
|------|------|-----------|-----------|----------|------|
| MODE_SAMPLE | 0x00 | 133MHz | 150MHz | 不需要 | 采样模式，验证ADC和数字捕获 |
| MODE_CRACK | 0x01 | 133MHz | 150MHz | 不需要 | 破译模式，侧信道数据采集验证 |
| MODE_HW_TEST | 0x02 | 133MHz | 150MHz | 不需要 | 硬件测试，验证温度和电压监控 |

**注：Demo版本不支持MODE_BRUTEFORCE暴力破解模式，不使用超频，以简化验证流程**

### 3.2 Demo版频率约束
| 项目 | 默认值 | 说明 |
|------|--------|------|
| RP2040系统时钟 | 133MHz | 固定不超频 |
| RP2350系统时钟 | 150MHz | 固定不超频 |
| ADC采样率 | 125KSPS | 最大规格，无需限制 |
| SPI波特率 | 20Mbps | 固定不变 |
| 数字捕获率 | 50MHz | PIO实现 |

---

## 4. Pico终端芯片（RP2040）引脚分配（Demo版）

### 4.1 引脚占用表

| GPIO编号 | 功能 | 方向 | 硬件说明 | Arduino引脚 |
|----------|------|------|----------|-------------|
| 0 | 数字输入通道0 | IN | 数字捕获通道0 | D0 |
| 1 | 数字输入通道1 | IN | 数字捕获通道1 | D1 |
| 2 | 数字输入通道2 | IN | 数字捕获通道2 | D2 |
| 3 | 数字输入通道3 | IN | 数字捕获通道3 | D3 |
| 4 | 数字输入通道4 | IN | 数字捕获通道4 | D4 |
| 5 | 数字输入通道5 | IN | 数字捕获通道5 | D5 |
| 6 | 数字输入通道6 | IN | 数字捕获通道6 | D6 |
| 7 | 数字输入通道7 | IN | 数字捕获通道7 | D7 |
| 16 | SPI MOSI | IN | 从Pico2接收数据 | - |
| 17 | SPI CS | IN | Pico2片选信号 | - |
| 18 | SPI SCK | IN | SPI时钟输入 | - |
| 19 | SPI MISO | OUT | 向Pico2发送数据 | - |
| 26 | ADC通道0 | IN | 模拟输入通道0 | A0 |
| 27 | ADC通道1 | IN | 模拟输入通道1 | A1 |
| 28 | ADC通道2 | IN | 模拟输入通道2 | A2 |
| 29 | ADC通道3 | IN | 模拟输入通道3 | A3 |
| 25 | LED指示 | OUT | 运行状态指示灯 | LED_BUILTIN |

### 4.2 功能区块

#### SPI从机接口
| 引脚 | 信号 | SPI端口 | Arduino SPI引脚号 |
|------|------|---------|-------------------|
| GPIO17 | CS | spi0 | PIN_SPI_CS |
| GPIO18 | SCK | spi0 | PIN_SPI_SCK |
| GPIO16 | MOSI | spi0 | PIN_SPI_MOSI |
| GPIO19 | MISO | spi0 | PIN_SPI_MISO |

#### ADC模拟采样
| 引脚 | ADC通道 | Arduino引脚 |
|------|---------|-------------|
| GPIO26 | ADC0 | A0 |
| GPIO27 | ADC1 | A1 |
| GPIO28 | ADC2 | A2 |
| GPIO29 | ADC3 | A3 |

#### 数字捕获
| 引脚 | 数字通道 | Arduino引脚 |
|------|---------|-------------|
| GPIO0 | D0 | D0 |
| GPIO1 | D1 | D1 |
| GPIO2 | D2 | D2 |
| GPIO3 | D3 | D3 |
| GPIO4 | D4 | D4 |
| GPIO5 | D5 | D5 |
| GPIO6 | D6 | D6 |
| GPIO7 | D7 | D7 |

---

## 5. Pico2协处理器（RP2350）引脚分配（Demo版）

### 5.1 引脚占用表

| GPIO编号 | 功能 | 方向 | 硬件说明 | Arduino引脚 |
|----------|------|------|----------|-------------|
| 2 | SPI CS0 | OUT | Pico#0片选 | D2 |
| 3 | SPI CS1 | OUT | Pico#1片选 | D3 |
| 4 | SPI CS2 | OUT | Pico#2片选 | D4 |
| 5 | SPI CS3 | OUT | Pico#3片选 | D5 |
| 6 | SPI CS4 | OUT | Pico#4片选 | D6 |
| 7 | SPI CS5 | OUT | Pico#5片选 | D7 |
| 8 | SPI CS6 | OUT | Pico#6片选 | D8 |
| 9 | SPI CS7 | OUT | Pico#7片选 | D9 |
| 10 | SPI SCK | OUT | SPI时钟输出 | PIN_SPI_SCK |
| 11 | SPI MOSI | OUT | SPI数据输出 | PIN_SPI_MOSI |
| 12 | SPI MISO | IN | SPI数据输入 | PIN_SPI_MISO |
| 25 | LED指示 | OUT | 运行状态指示灯 | LED_BUILTIN |

### 5.2 功能区块

#### SPI主机接口
| 引脚 | 信号 | SPI端口 | Arduino SPI引脚号 |
|------|------|---------|-------------------|
| GPIO10 | SCK | spi1 | PIN_SPI_SCK |
| GPIO11 | MOSI | spi1 | PIN_SPI_MOSI |
| GPIO12 | MISO | spi1 | PIN_SPI_MISO |
| GPIO2-9 | CS0-CS7 | - | D2-D9 |

#### USB CDC接口
- 使用RP2350内置USB外设，无需额外GPIO
- USB通信不受系统时钟影响

---

## 6. 电气规范

### 6.1 电压等级
| 信号类型 | 电压 | 说明 |
|----------|------|------|
| 模拟输入 | 0-3.3V | ADC参考电压3.3V |
| 数字输入 | 0-3.3V | CMOS电平 |
| SPI信号 | 3.3V | LVCMOS |

### 6.2 时序参数（Demo版，无超频）
| 参数 | 值 | 说明 |
|------|------|------|
| SPI时钟 | 20MHz | 固定不变 |
| ADC采样率 | 125KSPS | 最大规格 |
| 数字捕获率 | 50MHz | PIO实现 |
| RP2040系统时钟 | 133MHz | 固定 |
| RP2350系统时钟 | 150MHz | 固定 |

---

## 7. Arduino IDE代码适配示例（Demo版）

### 7.1 Pico (RP2040) Demo固件
```cpp
#include <Arduino.h>
#include <SPI.h>

// 引脚定义（Demo版）
#define SPI_CS_PIN    17
#define SPI_SCK_PIN   18
#define SPI_MOSI_PIN  16
#define SPI_MISO_PIN  19
#define DIGITAL_START_PIN 0
#define ADC_START_PIN      26

// 模式定义
#define MODE_SAMPLE   0x00
#define MODE_CRACK    0x01
#define MODE_HW_TEST  0x02

uint8_t current_mode = MODE_SAMPLE;
uint32_t sample_rate = 50000;
bool running = false;

void setup() {
    // 初始化SPI从机（使用GPIO16-19）
    SPI.setRX(SPI_MOSI_PIN);   // MOSI作为RX（从机接收）
    SPI.setTX(SPI_MISO_PIN);   // MISO作为TX（从机发送）
    SPI.setSCK(SPI_SCK_PIN);
    SPI.setCS(SPI_CS_PIN);
    SPI.begin(false);  // 从机模式
    
    // 初始化数字输入（GPIO0-7）
    for (int i = 0; i < 8; i++) {
        pinMode(DIGITAL_START_PIN + i, INPUT_PULLDOWN);
    }
    
    // 初始化ADC（GPIO26-29）
    analogReadResolution(12);
    for (int i = 0; i < 4; i++) {
        pinMode(ADC_START_PIN + i, INPUT);
    }
    
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
    // 检查CS引脚状态
    if (digitalRead(SPI_CS_PIN) == LOW) {
        // CS选中，处理SPI命令
        uint8_t rx_buf[32];
        uint8_t tx_buf[32];
        
        int count = SPI.transfer(rx_buf, tx_buf, 32);
        
        if (count > 0 && rx_buf[0] == 0xAA) {
            handle_command(rx_buf);
        }
    }
    
    if (running) {
        sample_and_process();
    }
    
    digitalWrite(LED_BUILTIN, running ? HIGH : LOW);
    delayMicroseconds(100);
}

void handle_command(uint8_t* cmd) {
    uint8_t cmd_code = cmd[3];
    uint8_t resp[128];
    uint16_t resp_len = 0;
    
    switch (cmd_code) {
        case 0x01:  // GET_STATUS
            resp[0] = current_mode;
            resp[1] = running ? 1 : 0;
            resp[2] = (sample_rate >> 0) & 0xFF;
            resp[3] = (sample_rate >> 8) & 0xFF;
            resp[4] = (sample_rate >> 16) & 0xFF;
            resp[5] = (sample_rate >> 24) & 0xFF;
            resp_len = 6;
            break;
            
        case 0x02:  // START_SAMPLE
            running = true;
            resp[0] = 0x01;
            resp_len = 1;
            break;
            
        case 0x03:  // STOP_SAMPLE
            running = false;
            resp[0] = 0x01;
            resp_len = 1;
            break;
            
        case 0x05:  // GET_DATA
            // 返回ADC和数字数据
            resp[0] = 0x01;  // DATA_ANALOG
            for (int i = 0; i < 4; i++) {
                uint16_t val = analogRead(ADC_START_PIN + i);
                resp[1 + i*2] = val & 0xFF;
                resp[2 + i*2] = (val >> 8) & 0xFF;
            }
            resp[9] = 0x02;  // DATA_DIGITAL
            resp[10] = digital_read_all();
            resp_len = 11;
            break;
            
        case 0x06:  // SET_MODE
            current_mode = cmd[4];
            resp[0] = 0x01;
            resp_len = 1;
            break;
    }
    
    send_response(resp, resp_len);
}

uint8_t digital_read_all() {
    uint8_t val = 0;
    for (int i = 0; i < 8; i++) {
        val |= (digitalRead(DIGITAL_START_PIN + i) ? 1 : 0) << i;
    }
    return val;
}

void send_response(uint8_t* data, uint16_t len) {
    uint8_t frame[128];
    frame[0] = 0xAA;
    frame[1] = len & 0xFF;
    frame[2] = (len >> 8) & 0xFF;
    memcpy(&frame[3], data, len);
    uint16_t crc = crc16(data, len);
    frame[3 + len] = crc & 0xFF;
    frame[4 + len] = (crc >> 8) & 0xFF;
    frame[5 + len] = 0x55;
    
    SPI.transfer(frame, 6 + len);
}

uint16_t crc16(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc;
}

void sample_and_process() {
    // Demo版简单采样循环
    static uint32_t last_sample = 0;
    if (micros() - last_sample >= 1000000 / sample_rate) {
        last_sample = micros();
        // 采样存储到缓冲区
    }
}
```

### 7.2 Pico2 (RP2350) Demo固件
```cpp
#include <Arduino.h>
#include <SPI.h>

#define NUM_PICO_SLAVES 8
#define SPI_CS_BASE     2

uint8_t pico_online[NUM_PICO_SLAVES] = {0};
bool running = false;

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
    
    // 检测Pico节点
    detect_pico_nodes();
    
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
    // 处理USB命令
    if (Serial.available()) {
        uint8_t cmd = Serial.read();
        handle_usb_command(cmd);
    }
    
    if (running) {
        aggregate_and_send();
    }
    
    digitalWrite(LED_BUILTIN, running ? HIGH : LOW);
    delay(1);
}

void detect_pico_nodes() {
    for (int i = 0; i < NUM_PICO_SLAVES; i++) {
        uint8_t resp[32];
        if (send_pico_command(i, 0x01, NULL, 0, resp)) {
            pico_online[i] = 1;
        }
        delay(1);
    }
}

bool send_pico_command(uint8_t slave_id, uint8_t cmd, 
                       uint8_t* params, uint8_t param_len,
                       uint8_t* resp) {
    if (slave_id >= NUM_PICO_SLAVES) return false;
    
    uint8_t tx_buf[32];
    uint8_t rx_buf[32];
    
    tx_buf[0] = 0xAA;
    tx_buf[1] = (1 + param_len) & 0xFF;
    tx_buf[2] = ((1 + param_len) >> 8) & 0xFF;
    tx_buf[3] = cmd;
    if (params && param_len > 0) {
        memcpy(&tx_buf[4], params, param_len);
    }
    tx_buf[4 + param_len] = 0x55;
    
    digitalWrite(SPI_CS_BASE + slave_id, LOW);
    delayMicroseconds(1);
    SPI.transfer(tx_buf, rx_buf, 32);
    delayMicroseconds(1);
    digitalWrite(SPI_CS_BASE + slave_id, HIGH);
    
    if (rx_buf[0] == 0xAA) {
        if (resp) {
            memcpy(resp, &rx_buf[3], rx_buf[1] | (rx_buf[2] << 8));
        }
        return true;
    }
    return false;
}

void aggregate_and_send() {
    uint8_t data_buf[256];
    uint32_t offset = 0;
    
    data_buf[offset++] = 0x10;  // DATA_AGGREGATED
    data_buf[offset++] = NUM_PICO_SLAVES;
    
    for (int i = 0; i < NUM_PICO_SLAVES; i++) {
        if (!pico_online[i]) continue;
        
        uint8_t resp[32];
        if (send_pico_command(i, 0x05, NULL, 0, resp)) {
            data_buf[offset++] = i;
            data_buf[offset++] = 11;  // 数据长度
            memcpy(&data_buf[offset], resp, 11);
            offset += 11;
        }
    }
    
    Serial.write(data_buf, offset);
}

void handle_usb_command(uint8_t cmd) {
    switch (cmd) {
        case 0x01:  // START
            for (int i = 0; i < NUM_PICO_SLAVES; i++) {
                if (pico_online[i]) {
                    send_pico_command(i, 0x02, NULL, 0, NULL);
                }
            }
            running = true;
            Serial.write(0x01);
            break;
            
        case 0x02:  // STOP
            for (int i = 0; i < NUM_PICO_SLAVES; i++) {
                if (pico_online[i]) {
                    send_pico_command(i, 0x03, NULL, 0, NULL);
                }
            }
            running = false;
            Serial.write(0x01);
            break;
            
        case 0x06:  // GET_STATUS
            uint8_t status[16];
            status[0] = running ? 1 : 0;
            status[1] = NUM_PICO_SLAVES;
            for (int i = 0; i < NUM_PICO_SLAVES; i++) {
                status[2 + i] = pico_online[i];
            }
            Serial.write(status, 10);
            break;
    }
}
```

---

## 8. 引脚冲突检测（Demo版）

### 8.1 Pico端冲突检查
| 功能 | 占用引脚 | 冲突检查 | 状态 |
|------|---------|----------|------|
| SPI从机 | GPIO16-19 | 无重叠 | ✅ 无冲突 |
| 数字捕获 | GPIO0-7 | 无重叠 | ✅ 无冲突 |
| ADC采样 | GPIO26-29 | 无重叠 | ✅ 无冲突 |
| LED | GPIO25 | 无重叠 | ✅ 无冲突 |

### 8.2 Pico2端冲突检查
| 功能 | 占用引脚 | 冲突检查 | 状态 |
|------|---------|----------|------|
| SPI主机 | GPIO10-12 | 无重叠 | ✅ 无冲突 |
| SPI CS | GPIO2-9 | 无重叠 | ✅ 无冲突 |
| LED | GPIO25 | 无重叠 | ✅ 无冲突 |

---

## 9. Demo版验证目标

### 9.1 验证项目清单
| 序号 | 验证项目 | 验证方法 | 预期结果 |
|------|----------|----------|----------|
| 1 | 信号采集分析 | 输入已知波形，采集并分析 | 波形数据正确采集 |
| 2 | 协议破解 | 模拟侧信道数据采集 | 数据聚合正常 |
| 3 | 硬件安全测试 | 温度监控和电压毛刺模拟 | 温度数据正常 |
| 4 | SPI通信 | 8片Pico轮询测试 | 所有节点响应正常 |
| 5 | USB通信 | 树莓派数据传输 | 数据完整无丢失 |

---

## 10. 硬件接口变更记录

| 版本 | 日期 | 变更内容 |
|------|------|----------|
| 1.0 | 2026-06-30 | 初始版本 |
| 1.1 | 2026-06-30 | 添加Arduino IDE开发环境适配、简化频率配置（无超频） |

---

## 11. Demo版与完整版差异说明

| 项目 | Demo版 | 完整版 |
|------|--------|--------|
| Pico数量 | 8片 | 16片 |
| I2C调度 | 不使用 | 使用I2C调度总线 |
| 超频 | 不支持 | 支持MODE_BRUTEFORCE超频 |
| 模式数量 | 3种（SAMPLE/CRACK/HW_TEST） | 4种（含BRUTEFORCE） |
| 集群扩展 | 固定8片 | 自适应扩展 |
| 开发环境 | Arduino IDE | Arduino IDE |