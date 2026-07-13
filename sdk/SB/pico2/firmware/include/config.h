#ifndef _PICO2_CONFIG_H
#define _PICO2_CONFIG_H

// 系统版本
#define FW_VERSION_MAJOR    1
#define FW_VERSION_MINOR    0
#define FW_VERSION_PATCH    0

// 集群配置
#define MAX_PICO_SLAVES     16      // 最大支持16片Pico
#define DEFAULT_PICO_COUNT  8       // 默认8片

// SPI配置
#define SPI_PORT            spi1
#define SPI_MOSI_PIN        11
#define SPI_MISO_PIN        12
#define SPI_SCK_PIN         10
#define SPI_CS_BASE_PIN     2       // CS0=GPIO2 ~ CS15=GPIO17
#define SPI_BAUDRATE        20000000  // 20Mbps

// I2C调度总线配置
#define I2C_SCHED_PORT      i2c0
#define I2C_SCHED_SDA_PIN   26
#define I2C_SCHED_SCL_PIN   27
#define I2C_SCHED_FREQ      400000   // 400kHz快速模式
#define I2C_SCHED_ADDR_BASE 0x40

// USB配置
#define USB_RX_BUFFER_SIZE  4096
#define USB_TX_BUFFER_SIZE  16384
#define USB_BAUDRATE        100000000

// 帧格式
#define FRAME_HEADER_PICO   0xAA
#define FRAME_TAIL_PICO     0x55
#define FRAME_HEADER_USB    0x55
#define FRAME_TAIL_USB      0xAA

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
#define CMD_START_CRACK     0x0F
#define CMD_START_TEST      0x10
#define CMD_ADDR_ASSIGN     0x11
#define CMD_ADDR_QUERY      0x12
#define CMD_ADDR_CLEAR      0x13
#define CMD_RESET           0xFF

// 数据类型
#define DATA_ANALOG         0x01
#define DATA_DIGITAL        0x02
#define DATA_STATUS         0x03
#define DATA_CRACK          0x04
#define DATA_AGGREGATED     0x10
#define DATA_CLUSTER_INFO   0x11
#define DATA_FAULT          0x12

// 工作模式
#define MODE_SAMPLE         0x00
#define MODE_CRACK          0x01
#define MODE_BRUTEFORCE     0x02
#define MODE_HW_TEST        0x03

// 时钟配置
#define DEFAULT_FREQ_KHZ    150000
#define OVERCLOCK_FREQ_KHZ  240000
#define VCORE_DEFAULT       VREG_VOLTAGE_DEFAULT
#define VCORE_OVERCLOCK     VREG_VOLTAGE_1_20

// 温度保护
#define TEMP_WARNING        60.0f
#define TEMP_SHUTDOWN       70.0f

// 故障检测
#define MAX_ERROR_COUNT     10
#define WATCHDOG_TIMEOUT    10000   // 10秒

// 缓冲区配置
#define AGGREGATE_BUF_SIZE  8192
#define NODE_DATA_BUF_SIZE  256

// FreeRTOS任务优先级
#define TASK_PRI_FOOLPROOF  6       // 防呆检测（最高）
#define TASK_PRI_SPI        5       // SPI通信
#define TASK_PRI_AGGREGATE  4       // 数据聚合
#define TASK_PRI_USB        4       // USB通信
#define TASK_PRI_TEMP       2       // 温度监控（最低）

#define TASK_STACK_FOOLPROOF 128
#define TASK_STACK_SPI       256
#define TASK_STACK_AGGREGATE 512
#define TASK_STACK_USB       512
#define TASK_STACK_TEMP      128

// 检测周期
#define FOOLPROOF_PERIOD_MS 10
#define SPI_PERIOD_MS       1
#define AGGREGATE_PERIOD_MS 1
#define USB_PERIOD_MS       2
#define TEMP_PERIOD_MS      100

#endif
