#include "i2c_sched_slave.h"

static TwoWire* g_i2c = NULL;
static uint8_t g_regs[32] = {0};
static uint8_t g_current_reg_addr = 0;
static uint8_t g_i2c_addr = I2C_ADDR_UNASSIGNED;
static sync_event_t g_event = SYNC_EVENT_NONE;
static bool g_regs_changed = false;
static void (*g_callback)(sync_event_t, uint8_t, uint8_t) = NULL;

static uint8_t calculate_checksum(void) {
    uint8_t sum = 0;
    for (int i = 0; i < REG_CHECKSUM; i++) {
        sum ^= g_regs[i];
    }
    for (int i = REG_CHECKSUM + 1; i < REG_COUNT; i++) {
        sum ^= g_regs[i];
    }
    return sum;
}

static void i2c_receive_handler(int count) {
    if (count == 0) return;
    
    uint8_t reg_addr = g_i2c->read();
    g_current_reg_addr = reg_addr;
    
    if (count > 1) {
        uint8_t value = g_i2c->read();
        g_regs[reg_addr] = value;
        
        if (reg_addr == REG_SYNC_TRIGGER) {
            if (value != 0) {
                g_event = SYNC_EVENT_START;
            } else {
                g_event = SYNC_EVENT_STOP;
            }
        } else if (reg_addr == REG_CTRL) {
            if (value & CTRL_RUN_MASK) {
                g_event = SYNC_EVENT_START;
            } else if (value & CTRL_STOP_MASK) {
                g_event = SYNC_EVENT_STOP;
            } else if (value & CTRL_RESET_MASK) {
                g_event = SYNC_EVENT_RESET;
            }
        }
        
        g_regs_changed = true;
        g_regs[REG_CHECKSUM] = calculate_checksum();
    }
}

static void i2c_request_handler(void) {
    g_i2c->write(g_regs[g_current_reg_addr]);
    g_current_reg_addr = (g_current_reg_addr + 1) % REG_COUNT;
}

void i2c_sched_slave_init(i2c_slave_config_t* config) {
    g_i2c = config->i2c_port;
    
    memset(g_regs, 0, sizeof(g_regs));
    g_regs[REG_CTRL] = CTRL_STOP_MASK;
    g_regs[REG_MODE] = MODE_SAMPLE;
    g_regs[REG_SAMPLE_RATE] = RATE_50KHZ;
    g_regs[REG_ADC_CH_SEL] = 0x0F;
    g_regs[REG_DIG_CH_SEL] = 0xFF;
    g_regs[REG_NODE_ID] = config->node_id;
    g_regs[REG_CLUSTER_ID] = config->cluster_id;
    g_regs[REG_CHECKSUM] = calculate_checksum();
    
    g_i2c->setSDA(config->sda_pin);
    g_i2c->setSCL(config->scl_pin);
    g_i2c->begin();
    g_i2c->setClock(config->bus_freq);
    
    if (!config->start_unassigned) {
        g_i2c_addr = I2C_ADDR_PICO(config->node_id);
        g_i2c->begin(g_i2c_addr);
        g_i2c->onReceive(i2c_receive_handler);
        g_i2c->onRequest(i2c_request_handler);
    }
}

void i2c_sched_slave_set_callback(void (*callback)(sync_event_t, uint8_t, uint8_t)) {
    g_callback = callback;
}

sync_event_t i2c_sched_slave_get_event(void) {
    return g_event;
}

void i2c_sched_slave_clear_event(void) {
    g_event = SYNC_EVENT_NONE;
}

bool i2c_sched_slave_regs_changed(void) {
    bool changed = g_regs_changed;
    g_regs_changed = false;
    return changed;
}

uint8_t i2c_sched_slave_get_reg(uint8_t reg_addr) {
    if (reg_addr > REG_RESERVED_END) return 0;
    return g_regs[reg_addr];
}

void i2c_sched_slave_update_status(uint8_t status_bit, bool value) {
    if (value) {
        g_regs[REG_STATUS] |= status_bit;
    } else {
        g_regs[REG_STATUS] &= ~status_bit;
    }
}

bool i2c_sched_slave_is_assigned(void) {
    return g_i2c_addr != I2C_ADDR_UNASSIGNED;
}

bool i2c_sched_slave_set_address(uint8_t addr) {
    if (g_i2c_addr != I2C_ADDR_UNASSIGNED) return false;
    
    g_i2c_addr = addr;
    g_i2c->begin(g_i2c_addr);
    g_i2c->onReceive(i2c_receive_handler);
    g_i2c->onRequest(i2c_request_handler);
    
    return true;
}

void i2c_sched_slave_clear_address(void) {
    g_i2c->end();
    g_i2c_addr = I2C_ADDR_UNASSIGNED;
}

uint8_t i2c_sched_slave_get_address(void) {
    return g_i2c_addr;
}
