#ifndef I2C_SCHED_MASTER_H
#define I2C_SCHED_MASTER_H

#include <Arduino.h>

#define MAX_PICO_SLAVES 8
#define I2C_SCHED_ADDR_BASE 0x40

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

typedef enum {
    SCHED_IDLE = 0,
    SCHED_INITIALIZING,
    SCHED_READY,
    SCHED_RUNNING,
    SCHED_ERROR
} sched_state_t;

typedef struct {
    TwoWire* i2c_port;
    uint8_t sda_pin;
    uint8_t scl_pin;
    uint32_t bus_freq;
    uint8_t max_nodes;
} i2c_master_config_t;

typedef struct {
    uint8_t node_id;
    uint8_t cluster_id;
    bool online;
    bool sync_locked;
    uint8_t status;
    uint8_t error;
    uint8_t version;
} i2c_node_state_t;

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

typedef struct {
    sched_state_t state;
    i2c_master_config_t config;
    i2c_node_state_t nodes[MAX_PICO_SLAVES];
    uint8_t node_count;
    sync_context_t sync_ctx;
    
    uint32_t last_sync_time;
    uint32_t sync_interval_us;
    
    uint8_t broadcast_buffer[32];
    uint8_t response_buffer[64];
} i2c_sched_ctx_t;

typedef enum {
    I2C_OK = 0,
    I2C_ERR_NACK = 1,
    I2C_ERR_INVALID_ADDR = 2,
    I2C_ERR_INVALID_REG = 3,
    I2C_ERR_DATA_OVERFLOW = 4,
    I2C_ERR_BUS_BUSY = 5
} i2c_status_t;

i2c_status_t i2c_sched_master_init(const i2c_master_config_t* config);
i2c_status_t i2c_sched_read_reg(uint8_t node_id, uint8_t reg_addr, uint8_t* value);
i2c_status_t i2c_sched_write_reg(uint8_t node_id, uint8_t reg_addr, uint8_t value);
i2c_status_t i2c_sched_read_regs(uint8_t node_id, uint8_t start_addr, 
                                  uint8_t* data, uint8_t count);
i2c_status_t i2c_sched_write_regs(uint8_t node_id, uint8_t start_addr, 
                                   const uint8_t* data, uint8_t count);
i2c_status_t i2c_sched_broadcast_write(uint8_t reg_addr, uint8_t value);
i2c_status_t i2c_sched_broadcast_write_regs(uint8_t start_addr, 
                                             const uint8_t* data, uint8_t count);
i2c_status_t i2c_sched_trigger_sync(void);
i2c_status_t i2c_sched_detect_nodes(void);
const i2c_node_state_t* i2c_sched_get_node_state(uint8_t node_id);
sched_state_t i2c_sched_get_state(void);
i2c_status_t i2c_sched_start(void);
i2c_status_t i2c_sched_stop(void);
void i2c_sched_sync_update(void);
void i2c_sched_master_deinit(void);

#endif
