#ifndef I2C_SCHED_REGS_H
#define I2C_SCHED_REGS_H

#include <Arduino.h>

#define I2C_ADDR_BASE        0x40
#define I2C_ADDR_UNASSIGNED  0xFF
#define I2C_ADDR_PICO(id)    (I2C_ADDR_BASE + (id))
#define I2C_BROADCAST_ADDR   0x3C

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

#define CTRL_RUN_MASK        0x01
#define CTRL_SYNC_MASK       0x02
#define CTRL_RESET_MASK      0x04
#define CTRL_STOP_MASK       0x08

#define TRIGGER_ACTIVE       0xAA
#define TRIGGER_INACTIVE     0x00

#define STATUS_RUNNING       0x01
#define STATUS_DATA_READY    0x02
#define STATUS_SYNC_LOCKED   0x04
#define STATUS_OVERCLOCKED   0x08
#define STATUS_HW_TEST_PASS  0x10
#define STATUS_BUSY          0x20
#define STATUS_WATCHDOG_OK   0x40
#define STATUS_INIT_COMPLETE 0x80

#define I2C_OK               0x00
#define I2C_ERR_NACK         0x01
#define I2C_ERR_INVALID_ADDR 0x02
#define I2C_ERR_INVALID_REG  0x03
#define I2C_ERR_DATA_OVERFLOW 0x04
#define I2C_ERR_BUS_BUSY     0x05

typedef enum {
    SYNC_EVENT_NONE = 0,
    SYNC_EVENT_START = 1,
    SYNC_EVENT_STOP = 2,
    SYNC_EVENT_RESET = 3
} sync_event_t;

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

typedef struct {
    bool sync_locked;
    uint32_t sync_timestamp;
    uint32_t last_trigger;
    uint8_t trigger_count;
} sync_context_t;

uint8_t i2c_regs_calculate_checksum(const i2c_regs_t* regs);
bool i2c_regs_validate_checksum(const i2c_regs_t* regs);
void i2c_regs_reset(i2c_regs_t* regs);
void i2c_regs_init(i2c_regs_t* regs, uint8_t node_id, uint8_t cluster_id);

#endif
