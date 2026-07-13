#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#define FW_VERSION_MAJOR    1
#define FW_VERSION_MINOR    0
#define FW_VERSION_PATCH    0

#define MAX_PICO_SLAVES     16
#define DEFAULT_PICO_COUNT  8

#define SPI_PORT            1
#define SPI_MOSI_PIN        11
#define SPI_MISO_PIN        12
#define SPI_SCK_PIN         10
#define SPI_CS_BASE_PIN     2
#define SPI_BAUDRATE        20000000

#define I2C_SCHED_PORT      0
#define I2C_SCHED_SDA_PIN   26
#define I2C_SCHED_SCL_PIN   27
#define I2C_SCHED_FREQ      400000
#define I2C_SCHED_ADDR_BASE 0x40

#define USB_BAUDRATE        100000000

#define FRAME_HEADER_PICO   0xAA
#define FRAME_TAIL_PICO     0x55
#define FRAME_HEADER_USB    0x55
#define FRAME_TAIL_USB      0xAA

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

#define DATA_ANALOG         0x01
#define DATA_DIGITAL        0x02
#define DATA_STATUS         0x03
#define DATA_CRACK          0x04
#define DATA_AGGREGATED     0x10
#define DATA_CLUSTER_INFO   0x11
#define DATA_FAULT          0x12

#define MODE_SAMPLE         0x00
#define MODE_CRACK          0x01
#define MODE_BRUTEFORCE     0x02
#define MODE_HW_TEST        0x03

#define DEFAULT_FREQ_KHZ    150000
#define OVERCLOCK_FREQ_KHZ  240000

#define DEFAULT_SAMPLE_RATE 50000

#define TEMP_WARNING        60.0f
#define TEMP_SHUTDOWN       70.0f

#define MAX_ERROR_COUNT     10
#define WATCHDOG_TIMEOUT    10000

#define AGGREGATE_BUF_SIZE  8192
#define NODE_DATA_BUF_SIZE  256

#define TEMP_PERIOD_MS      100

#endif