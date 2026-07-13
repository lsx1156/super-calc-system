#ifndef _SUPER_CALC_CONFIG_H
#define _SUPER_CALC_CONFIG_H

#include "hardware/i2c.h"
#include "hardware/spi.h"

// 系统版本
#define FW_VERSION_MAJOR    1
#define FW_VERSION_MINOR    0
#define FW_VERSION_PATCH    0

// 硬件配置
#define ADC_CHANNELS        4       // 4路ADC
#define DIGITAL_CHANNELS    8       // 8路数字输入
#define SPI_PORT            spi0    // SPI端口
#define SPI_CS_PIN          1       // SPI CS引脚
#define SPI_SCK_PIN         2       // SPI SCK引脚
#define SPI_MOSI_PIN        3       // SPI MOSI引脚
#define SPI_MISO_PIN        4       // SPI MISO引脚
#define SPI_BAUDRATE        20000000 // 20Mbps

// I2C调度总线配置
#define I2C_SCHED_PORT      i2c0
#define I2C_SCHED_SDA_PIN   0       // I2C SDA引脚
#define I2C_SCHED_SCL_PIN   5       // I2C SCL引脚
#define I2C_SCHED_FREQ      400000   // 400kHz快速模式
#define NODE_ID             0       // 默认节点ID，由Pico2配置

// 采样配置
#define DEFAULT_SAMPLE_RATE 50000   // 默认采样率
#define MAX_SAMPLE_RATE     125000  // 最大采样率
#define ADC_BUFFER_SIZE     4096    // ADC缓冲区大小
#define ADC_DMA_BLOCK_SIZE  512     // ADC DMA块传输大小
#define DIGITAL_BUFFER_SIZE 4096    // 数字缓冲区大小

// 采样率编码
#define RATE_1KHZ           0x00
#define RATE_5KHZ           0x01
#define RATE_10KHZ          0x02
#define RATE_25KHZ          0x03
#define RATE_50KHZ          0x04
#define RATE_100KHZ         0x05
#define RATE_125KHZ         0x06

#define RATE_TO_HZ(code) ((code == RATE_1KHZ) ? 1000 : \
                         (code == RATE_5KHZ) ? 5000 : \
                         (code == RATE_10KHZ) ? 10000 : \
                         (code == RATE_25KHZ) ? 25000 : \
                         (code == RATE_50KHZ) ? 50000 : \
                         (code == RATE_100KHZ) ? 100000 : \
                         (code == RATE_125KHZ) ? 125000 : 50000)

// 时钟配置
#define DEFAULT_FREQ_KHZ    133000  // 默认频率
#define OVERCLOCK_FREQ_KHZ  200000  // 超频频率
#define VCORE_DEFAULT       VREG_VOLTAGE_DEFAULT
#define VCORE_OVERCLOCK     VREG_VOLTAGE_1_20

// 超频模式
#define OC_DISABLED         0x00
#define OC_167MHZ           0x01
#define OC_200MHZ           0x02
#define OC_AUTO             0x03

// 工作模式
#define MODE_SAMPLE         0x00    // 采样模式
#define MODE_CRACK          0x01    // 破译模式
#define MODE_BRUTEFORCE     0x02    // 暴力破解模式
#define MODE_HW_TEST        0x03    // 硬件测试模式

// 命令码
#define CMD_NOP             0x00
#define CMD_GET_STATUS      0x01
#define CMD_START_SAMPLE    0x02
#define CMD_STOP_SAMPLE     0x03
#define CMD_SET_RATE        0x04
#define CMD_GET_DATA        0x05
#define CMD_SET_MODE        0x06
#define CMD_GET_VERSION     0x07
#define CMD_OVERCLOCK       0x08
#define CMD_HW_TEST         0x09
#define CMD_GLITCH          0x0A
#define CMD_NODE_DETECT     0x0B
#define CMD_SET_NODE_COUNT  0x0C
#define CMD_RESET_NODE      0x0D
#define CMD_BROADCAST       0x0E
#define CMD_RESET           0xFF

// 帧格式
#define FRAME_HEADER        0xAA
#define FRAME_TAIL          0x55

// 数据类型
#define DATA_ANALOG         0x01
#define DATA_DIGITAL        0x02
#define DATA_STATUS         0x03
#define DATA_CRACK          0x04

// 温度保护
#define TEMP_WARNING        60.0f
#define TEMP_SHUTDOWN       70.0f

// 看门狗
#define WATCHDOG_TIMEOUT    8000    // 8秒

// FreeRTOS任务优先级
#define TASK_PRI_SPI        4       // SPI通信（高）
#define TASK_PRI_SAMPLE     3       // 采样（中高）
#define TASK_PRI_CRACK      2       // 破解（中）
#define TASK_PRI_MONITOR    1       // 监控（低）

#define TASK_STACK_SPI      256
#define TASK_STACK_SAMPLE   256
#define TASK_STACK_CRACK    512
#define TASK_STACK_MONITOR  128

#endif
