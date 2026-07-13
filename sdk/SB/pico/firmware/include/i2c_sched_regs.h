#ifndef _I2C_SCHED_REGS_H
#define _I2C_SCHED_REGS_H

#include <stdint.h>
#include <stdbool.h>

// ============================================
// I2C调度总线 - 共享寄存器定义
// ============================================
// 架构：Pico2作为I2C主机，Pico作为I2C从机
// 每个Pico拥有独立的寄存器空间
// I2C从机地址：0x40 + node_id (0x40 ~ 0x4F, 支持16个Pico)
// 通信速率：400kHz (快速模式)
// ============================================

// -------------------- I2C地址分配 --------------------
#define I2C_ADDR_BASE        0x40
#define I2C_ADDR_MAX         0x4F
#define I2C_ADDR_PICO(n)     (I2C_ADDR_BASE + (n))

#define I2C_BUS_FREQ         400000

// -------------------- 寄存器映射 --------------------
// 所有寄存器均为8位，采用字节寻址

#define REG_CTRL             0x00
#define REG_MODE             0x01
#define REG_SAMPLE_RATE      0x02
#define REG_ADC_CH_SEL       0x03
#define REG_DIG_CH_SEL       0x04
#define REG_SYNC_TRIGGER     0x05
#define REG_STATUS           0x06
#define REG_ERROR            0x07
#define REG_SYNC_TIME_0      0x08
#define REG_SYNC_TIME_1      0x09
#define REG_SYNC_TIME_2      0x0A
#define REG_SYNC_TIME_3      0x0B
#define REG_CHECKSUM         0x0C
#define REG_VERSION          0x0D
#define REG_NODE_ID          0x0E
#define REG_CLUSTER_ID       0x0F
#define REG_OVERCLOCK        0x10
#define REG_RESERVED_START   0x11
#define REG_RESERVED_END     0x1F

#define REG_ADDR_MIN         REG_CTRL
#define REG_ADDR_MAX         REG_RESERVED_END
#define REG_COUNT            (REG_RESERVED_END + 1)

// -------------------- REG_CTRL (控制寄存器) --------------------
#define CTRL_RUN_MASK        0x01
#define CTRL_RUN_SHIFT       0
#define CTRL_SYNC_MASK       0x02
#define CTRL_SYNC_SHIFT      1
#define CTRL_RESET_MASK      0x04
#define CTRL_RESET_SHIFT     2
#define CTRL_STOP_MASK       0x08
#define CTRL_STOP_SHIFT      3
#define CTRL_CLEAR_ERR_MASK  0x10
#define CTRL_CLEAR_ERR_SHIFT 4
#define CTRL_AUTO_RESTART    0x20
#define CTRL_BROADCAST_MASK  0x80

#define CTRL_GET_RUN(r)      ((r & CTRL_RUN_MASK) >> CTRL_RUN_SHIFT)
#define CTRL_GET_SYNC(r)     ((r & CTRL_SYNC_MASK) >> CTRL_SYNC_SHIFT)
#define CTRL_GET_RESET(r)    ((r & CTRL_RESET_MASK) >> CTRL_RESET_SHIFT)
#define CTRL_GET_STOP(r)     ((r & CTRL_STOP_MASK) >> CTRL_STOP_SHIFT)

#define CTRL_SET_RUN(r, v)   ((r & ~CTRL_RUN_MASK) | ((v) << CTRL_RUN_SHIFT))
#define CTRL_SET_SYNC(r, v)  ((r & ~CTRL_SYNC_MASK) | ((v) << CTRL_SYNC_SHIFT))
#define CTRL_SET_RESET(r, v) ((r & ~CTRL_RESET_MASK) | ((v) << CTRL_RESET_SHIFT))

// -------------------- REG_MODE (模式寄存器) --------------------
#define MODE_SAMPLE          0x00
#define MODE_CRACK           0x01
#define MODE_BRUTEFORCE      0x02
#define MODE_HW_TEST         0x03
#define MODE_DIAGNOSTIC      0x04
#define MODE_RESERVED_START  0x05

// -------------------- REG_SAMPLE_RATE (采样率寄存器) --------------------
// 采样率编码表
#define RATE_1KHZ            0x00
#define RATE_5KHZ            0x01
#define RATE_10KHZ           0x02
#define RATE_25KHZ           0x03
#define RATE_50KHZ           0x04
#define RATE_100KHZ          0x05
#define RATE_125KHZ          0x06

#define RATE_TO_HZ(code) ( \
    (code) == RATE_1KHZ   ? 1000   : \
    (code) == RATE_5KHZ   ? 5000   : \
    (code) == RATE_10KHZ  ? 10000  : \
    (code) == RATE_25KHZ  ? 25000  : \
    (code) == RATE_50KHZ  ? 50000  : \
    (code) == RATE_100KHZ ? 100000 : \
    (code) == RATE_125KHZ ? 125000 : 50000 \
)

// -------------------- REG_ADC_CH_SEL (ADC通道选择) --------------------
#define ADC_CH0_MASK         0x01
#define ADC_CH1_MASK         0x02
#define ADC_CH2_MASK         0x04
#define ADC_CH3_MASK         0x08
#define ADC_ALL_CH           0x0F

// -------------------- REG_DIG_CH_SEL (数字通道选择) --------------------
#define DIG_CH0_3_MASK       0x0F
#define DIG_CH4_7_MASK       0xF0
#define DIG_ALL_CH           0xFF

// -------------------- REG_SYNC_TRIGGER (同步触发寄存器) --------------------
// 写入任意非零值触发同步采样
#define TRIGGER_ACTIVE       0xAA
#define TRIGGER_INACTIVE     0x00
#define TRIGGER_BROADCAST    0xFF

// -------------------- REG_STATUS (状态寄存器) --------------------
#define STATUS_RUNNING       0x01
#define STATUS_DATA_READY    0x02
#define STATUS_SYNC_LOCKED   0x04
#define STATUS_OVERCLOCKED   0x08
#define STATUS_HW_TEST_PASS  0x10
#define STATUS_BUSY          0x20
#define STATUS_WATCHDOG_OK   0x40
#define STATUS_INIT_COMPLETE 0x80

// -------------------- REG_ERROR (错误寄存器) --------------------
#define ERR_NONE             0x00
#define ERR_ADC_TIMEOUT      0x01
#define ERR_DIG_TIMEOUT      0x02
#define ERR_SPI_COMM         0x04
#define ERR_I2C_COMM         0x08
#define ERR_OVERHEAT         0x10
#define ERR_OVERCLOCK_FAIL   0x20
#define ERR_DATA_CORRUPT     0x40
#define ERR_UNKNOWN          0x80

// -------------------- REG_SYNC_TIME_L/H (同步时间戳) --------------------
// Pico2广播的同步时间戳，低16位和高16位
// 使用time_us_32()获取微秒级时间

// -------------------- REG_CHECKSUM (校验和) --------------------
// 寄存器校验和，用于检测通信错误

// -------------------- REG_VERSION (版本寄存器) --------------------
#define FW_VER_MAJOR_SHIFT   4
#define FW_VER_MINOR_SHIFT   0

// -------------------- REG_NODE_ID (节点ID) --------------------
// 0-15，同一Pico2下的Pico编号

// -------------------- REG_CLUSTER_ID (集群ID) --------------------
// 0-7，Pico2编号

// -------------------- REG_OVERCLOCK (超频控制) --------------------
#define OC_DISABLED          0x00
#define OC_167MHZ            0x01
#define OC_200MHZ            0x02
#define OC_AUTO              0x03

// ============================================
// I2C调度帧格式
// ============================================
// 
// 单节点写入 (Pico2 -> Pico):
// ┌─────────┬───────────┬───────────┬───────────────┐
// │ S + ADDR │ W + ACK  │ REG_ADDR  │ DATA + ACK    │
// │ (1+1)    │ (1+1)    │ (1+1)     │ (N+N)         │
// └─────────┴───────────┴───────────┴───────────────┘
// 
// 单节点读取 (Pico2 <- Pico):
// ┌─────────┬───────────┬───────────┬───────────┬───────────────┐
// │ S + ADDR │ W + ACK  │ REG_ADDR  │ R + ACK  │ DATA + NACK + P│
// │ (1+1)    │ (1+1)    │ (1+1)     │ (1+1)    │ (N+N+1)       │
// └─────────┴───────────┴───────────┴───────────┴───────────────┘
// 
// 广播写入 (Pico2 -> 所有Pico):
// ┌─────────┬───────────┬───────────┬───────────────┐
// │ S + 0x00 │ W + ACK  │ REG_ADDR  │ DATA          │
// │ (1+1)    │ (1+1)    │ (1+1)     │ (N)           │
// └─────────┴───────────┴───────────┴───────────────┘
// 
// S: Start, P: Stop, ACK: Acknowledge, NACK: Not Acknowledge
// ADDR: 从机地址 (7位)
// REG_ADDR: 寄存器地址 (8位)
// DATA: 数据 (N字节)
// 
// ============================================

// -------------------- 广播地址 --------------------
#define I2C_BROADCAST_ADDR   0x00

// -------------------- 帧格式常量 --------------------
#define I2C_FRAME_MIN_LEN    3
#define I2C_FRAME_MAX_LEN    16

// -------------------- 状态码 --------------------
typedef enum {
    I2C_OK = 0,
    I2C_ERR_NACK,
    I2C_ERR_TIMEOUT,
    I2C_ERR_BUS_BUSY,
    I2C_ERR_INVALID_ADDR,
    I2C_ERR_INVALID_REG,
    I2C_ERR_DATA_OVERFLOW,
    I2C_ERR_UNKNOWN
} i2c_status_t;

// -------------------- 寄存器结构体 --------------------
typedef struct {
    uint8_t ctrl;
    uint8_t mode;
    uint8_t sample_rate;
    uint8_t adc_ch_sel;
    uint8_t dig_ch_sel;
    uint8_t sync_trigger;
    uint8_t status;
    uint8_t error;
    uint8_t sync_time[4];
    uint8_t checksum;
    uint8_t version;
    uint8_t node_id;
    uint8_t cluster_id;
    uint8_t overclock;
    uint8_t reserved[15];
} i2c_regs_t;

// -------------------- 同步事件类型 --------------------
typedef enum {
    SYNC_EVENT_NONE = 0,
    SYNC_EVENT_START,
    SYNC_EVENT_STOP,
    SYNC_EVENT_RESET,
    SYNC_EVENT_DATA_READY,
    SYNC_EVENT_ERROR
} sync_event_t;

// -------------------- 同步上下文 --------------------
typedef struct {
    bool sync_locked;
    uint32_t sync_timestamp;
    uint32_t last_trigger;
    uint8_t trigger_count;
} sync_context_t;

// -------------------- 函数声明 --------------------
uint8_t i2c_regs_calculate_checksum(const i2c_regs_t* regs);
bool i2c_regs_validate_checksum(const i2c_regs_t* regs);
void i2c_regs_reset(i2c_regs_t* regs);
void i2c_regs_init(i2c_regs_t* regs, uint8_t node_id, uint8_t cluster_id);

#endif
