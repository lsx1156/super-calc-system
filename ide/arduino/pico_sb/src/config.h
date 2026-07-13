#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#define FW_VERSION_MAJOR    1
#define FW_VERSION_MINOR    0
#define FW_VERSION_PATCH    0

#define ADC_CHANNELS        4
#define DIGITAL_CHANNELS    8

#define SPI_PORT            0
#define SPI_CS_PIN          1
#define SPI_SCK_PIN         2
#define SPI_MOSI_PIN        3
#define SPI_MISO_PIN        4
#define SPI_BAUDRATE        20000000

#define I2C_SCHED_PORT      0
#define I2C_SCHED_SDA_PIN   0
#define I2C_SCHED_SCL_PIN   5
#define I2C_SCHED_FREQ      400000
#define NODE_ID             0

#define DEFAULT_SAMPLE_RATE 50000
#define MAX_SAMPLE_RATE     125000
#define ADC_BUFFER_SIZE     4096
#define ADC_DMA_BLOCK_SIZE  512
#define DIGITAL_BUFFER_SIZE 4096

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

#define DEFAULT_FREQ_KHZ    133000
#define OVERCLOCK_FREQ_KHZ  200000

#define OC_DISABLED         0x00
#define OC_167MHZ           0x01
#define OC_200MHZ           0x02
#define OC_AUTO             0x03

#define MODE_SAMPLE         0x00
#define MODE_CRACK          0x01
#define MODE_BRUTEFORCE     0x02
#define MODE_HW_TEST        0x03

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

#define FRAME_HEADER        0xAA
#define FRAME_TAIL          0x55

#define DATA_ANALOG         0x01
#define DATA_DIGITAL        0x02
#define DATA_STATUS         0x03
#define DATA_CRACK          0x04

#define TEMP_WARNING        60.0f
#define TEMP_SHUTDOWN       70.0f

#define WATCHDOG_TIMEOUT    8000

#define DIGITAL_PIN_START   6

#define I2C_ADDR_BASE        0x40
#define I2C_ADDR_UNASSIGNED  0xFF
#define I2C_ADDR_PICO(id)    (I2C_ADDR_BASE + (id))
#define I2C_BROADCAST_ADDR   0x3C

#define ADC_ALL_CH           0x0F
#define DIG_ALL_CH           0xFF

#define FW_VER_MAJOR_SHIFT   4

#endif