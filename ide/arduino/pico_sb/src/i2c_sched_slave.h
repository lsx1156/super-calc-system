#ifndef I2C_SCHED_SLAVE_H
#define I2C_SCHED_SLAVE_H

#include <Arduino.h>

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

#define REG_COUNT            (REG_RESERVED_END + 1)

#define CTRL_RUN_MASK        0x01
#define CTRL_SYNC_MASK       0x02
#define CTRL_RESET_MASK      0x04
#define CTRL_STOP_MASK       0x08
#define CTRL_CLEAR_ERR_MASK  0x10
#define CTRL_AUTO_RESTART    0x20
#define CTRL_BROADCAST_MASK  0x80

#define MODE_SAMPLE          0x00
#define MODE_CRACK           0x01
#define MODE_BRUTEFORCE      0x02
#define MODE_HW_TEST         0x03
#define MODE_DIAGNOSTIC      0x04

#define RATE_1KHZ            0x00
#define RATE_5KHZ            0x01
#define RATE_10KHZ           0x02
#define RATE_25KHZ           0x03
#define RATE_50KHZ           0x04
#define RATE_100KHZ          0x05
#define RATE_125KHZ          0x06

#define STATUS_RUNNING       0x01
#define STATUS_DATA_READY    0x02
#define STATUS_SYNC_LOCKED   0x04
#define STATUS_OVERCLOCKED   0x08
#define STATUS_HW_TEST_PASS  0x10
#define STATUS_BUSY          0x20
#define STATUS_WATCHDOG_OK   0x40
#define STATUS_INIT_COMPLETE 0x80

#define OC_DISABLED          0x00
#define OC_167MHZ            0x01
#define OC_200MHZ            0x02
#define OC_AUTO              0x03

#define I2C_ADDR_BASE        0x40
#define I2C_ADDR_PICO(n)     (I2C_ADDR_BASE + (n))
#define I2C_ADDR_UNASSIGNED 0xFF

#define SLOT_TO_I2C_ADDR(slot) (I2C_ADDR_BASE + (slot))

typedef enum {
    SYNC_EVENT_NONE = 0,
    SYNC_EVENT_START,
    SYNC_EVENT_STOP,
    SYNC_EVENT_RESET,
    SYNC_EVENT_DATA_READY,
    SYNC_EVENT_ERROR
} sync_event_t;

typedef struct {
    TwoWire* i2c_port;
    uint8_t sda_pin;
    uint8_t scl_pin;
    uint8_t node_id;
    uint8_t cluster_id;
    uint32_t bus_freq;
    bool start_unassigned;
} i2c_slave_config_t;

void i2c_sched_slave_init(i2c_slave_config_t* config);
void i2c_sched_slave_set_callback(void (*callback)(sync_event_t, uint8_t, uint8_t));
sync_event_t i2c_sched_slave_get_event(void);
void i2c_sched_slave_clear_event(void);
bool i2c_sched_slave_regs_changed(void);
uint8_t i2c_sched_slave_get_reg(uint8_t reg_addr);
void i2c_sched_slave_update_status(uint8_t status_bit, bool value);
bool i2c_sched_slave_is_assigned(void);
bool i2c_sched_slave_set_address(uint8_t addr);
void i2c_sched_slave_clear_address(void);
uint8_t i2c_sched_slave_get_address(void);

#endif