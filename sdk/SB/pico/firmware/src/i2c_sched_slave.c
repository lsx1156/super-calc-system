#include "i2c_sched_slave.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/i2c_slave.h"
#include "config.h"
#include <string.h>

static i2c_slave_ctx_t g_slave_ctx;
static i2c_sync_callback_t g_sync_callback = NULL;

static void i2c_slave_handler(i2c_inst_t* i2c, i2c_slave_event_t event) {
    uint8_t* reg_ptr = (uint8_t*)&g_slave_ctx.regs;
    
    switch (event) {
        case I2C_SLAVE_RECEIVE:
            if (!g_slave_ctx.addr_written) {
                g_slave_ctx.current_reg_addr = i2c_read_byte_raw(i2c);
                g_slave_ctx.addr_written = true;
            } else {
                if (g_slave_ctx.current_reg_addr <= REG_RESERVED_END) {
                    uint8_t value = i2c_read_byte_raw(i2c);
                    
                    if (g_slave_ctx.current_reg_addr == REG_CTRL) {
                        g_slave_ctx.regs.ctrl = value;
                        
                        if (value & CTRL_RUN_MASK) {
                            g_slave_ctx.state = SLAVE_RUNNING;
                            g_slave_ctx.pending_event = SYNC_EVENT_START;
                        } else if (value & CTRL_STOP_MASK) {
                            g_slave_ctx.state = SLAVE_READY;
                            g_slave_ctx.pending_event = SYNC_EVENT_STOP;
                        } else if (value & CTRL_RESET_MASK) {
                            i2c_regs_reset(&g_slave_ctx.regs);
                            g_slave_ctx.state = SLAVE_READY;
                            g_slave_ctx.pending_event = SYNC_EVENT_RESET;
                        } else if (value & CTRL_CLEAR_ERR_MASK) {
                            g_slave_ctx.regs.error = ERR_NONE;
                        }
                        
                        if (g_sync_callback) {
                            g_sync_callback(g_slave_ctx.pending_event, REG_CTRL, value);
                        }
                    } else if (g_slave_ctx.current_reg_addr == REG_SYNC_TRIGGER) {
                        g_slave_ctx.regs.sync_trigger = value;
                        
                        if (value == TRIGGER_ACTIVE) {
                            uint32_t sync_time = ((uint32_t)g_slave_ctx.regs.sync_time[3] << 24) |
                                                 ((uint32_t)g_slave_ctx.regs.sync_time[2] << 16) |
                                                 ((uint32_t)g_slave_ctx.regs.sync_time[1] << 8) |
                                                 ((uint32_t)g_slave_ctx.regs.sync_time[0]);
                            g_slave_ctx.sync_ctx.sync_timestamp = sync_time;
                            g_slave_ctx.sync_ctx.sync_locked = true;
                            g_slave_ctx.sync_ctx.trigger_count++;
                            g_slave_ctx.regs.status |= STATUS_SYNC_LOCKED;
                            g_slave_ctx.pending_event = SYNC_EVENT_START;
                            
                            if (g_sync_callback) {
                                g_sync_callback(SYNC_EVENT_START, REG_SYNC_TRIGGER, TRIGGER_ACTIVE);
                            }
                        } else if (value == TRIGGER_INACTIVE) {
                            g_slave_ctx.regs.sync_trigger = TRIGGER_INACTIVE;
                        }
                    } else if (g_slave_ctx.current_reg_addr == REG_MODE) {
                        g_slave_ctx.regs.mode = value;
                        g_slave_ctx.regs_dirty = true;
                        
                        if (g_sync_callback) {
                            g_sync_callback(SYNC_EVENT_NONE, REG_MODE, value);
                        }
                    } else if (g_slave_ctx.current_reg_addr == REG_SAMPLE_RATE) {
                        g_slave_ctx.regs.sample_rate = value;
                        g_slave_ctx.regs_dirty = true;
                        
                        if (g_sync_callback) {
                            g_sync_callback(SYNC_EVENT_NONE, REG_SAMPLE_RATE, value);
                        }
                    } else if (g_slave_ctx.current_reg_addr == REG_OVERCLOCK) {
                        g_slave_ctx.regs.overclock = value;
                        g_slave_ctx.regs_dirty = true;
                        
                        if (g_sync_callback) {
                            g_sync_callback(SYNC_EVENT_NONE, REG_OVERCLOCK, value);
                        }
                    } else if (g_slave_ctx.current_reg_addr == REG_ADC_CH_SEL) {
                        g_slave_ctx.regs.adc_ch_sel = value;
                        g_slave_ctx.regs_dirty = true;
                        
                        if (g_sync_callback) {
                            g_sync_callback(SYNC_EVENT_NONE, REG_ADC_CH_SEL, value);
                        }
                    } else if (g_slave_ctx.current_reg_addr == REG_DIG_CH_SEL) {
                        g_slave_ctx.regs.dig_ch_sel = value;
                        g_slave_ctx.regs_dirty = true;
                        
                        if (g_sync_callback) {
                            g_sync_callback(SYNC_EVENT_NONE, REG_DIG_CH_SEL, value);
                        }
                    } else {
                        reg_ptr[g_slave_ctx.current_reg_addr] = value;
                    }
                    
                    g_slave_ctx.regs.checksum = i2c_regs_calculate_checksum(&g_slave_ctx.regs);
                    g_slave_ctx.current_reg_addr++;
                }
            }
            break;
            
        case I2C_SLAVE_REQUEST:
            if (g_slave_ctx.current_reg_addr <= REG_RESERVED_END) {
                i2c_write_byte_raw(i2c, reg_ptr[g_slave_ctx.current_reg_addr]);
            } else {
                i2c_write_byte_raw(i2c, 0x00);
            }
            g_slave_ctx.current_reg_addr = (g_slave_ctx.current_reg_addr + 1) % REG_COUNT;
            break;
            
        case I2C_SLAVE_FINISH:
            g_slave_ctx.addr_written = false;
            break;
            
        default:
            break;
    }
}

i2c_status_t i2c_sched_slave_init(const i2c_slave_config_t* config) {
    memset(&g_slave_ctx, 0, sizeof(i2c_slave_ctx_t));
    
    if (!config || !config->i2c_port) {
        return I2C_ERR_INVALID_ADDR;
    }
    
    g_slave_ctx.config = *config;
    g_slave_ctx.state = SLAVE_INITIALIZING;
    g_slave_ctx.addr_written = false;
    g_slave_ctx.pending_event = SYNC_EVENT_NONE;
    g_slave_ctx.regs_dirty = false;
    
    i2c_regs_init(&g_slave_ctx.regs, config->node_id, config->cluster_id);
    g_slave_ctx.regs.status |= STATUS_INIT_COMPLETE;
    
    i2c_init(config->i2c_port, config->bus_freq);
    
    gpio_set_function(config->sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(config->scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(config->sda_pin);
    gpio_pull_up(config->scl_pin);
    
    if (config->start_unassigned) {
        g_slave_ctx.state = SLAVE_UNASSIGNED;
        g_slave_ctx.regs.node_id = 0xFF;
        g_slave_ctx.regs.cluster_id = 0xFF;
    } else {
        uint8_t i2c_addr = I2C_ADDR_PICO(config->node_id);
        i2c_slave_init(config->i2c_port, i2c_addr, i2c_slave_handler);
        g_slave_ctx.state = SLAVE_READY;
    }
    
    return I2C_OK;
}

void i2c_sched_slave_set_callback(i2c_sync_callback_t callback) {
    g_sync_callback = callback;
}

uint8_t i2c_sched_slave_get_reg(uint8_t reg_addr) {
    if (reg_addr > REG_RESERVED_END) {
        return 0;
    }
    
    uint8_t* reg_ptr = (uint8_t*)&g_slave_ctx.regs;
    return reg_ptr[reg_addr];
}

void i2c_sched_slave_set_reg(uint8_t reg_addr, uint8_t value) {
    if (reg_addr > REG_RESERVED_END) {
        return;
    }
    
    uint8_t* reg_ptr = (uint8_t*)&g_slave_ctx.regs;
    reg_ptr[reg_addr] = value;
    g_slave_ctx.regs.checksum = i2c_regs_calculate_checksum(&g_slave_ctx.regs);
}

void i2c_sched_slave_update_status(uint8_t status_bit, bool set) {
    if (set) {
        g_slave_ctx.regs.status |= status_bit;
    } else {
        g_slave_ctx.regs.status &= ~status_bit;
    }
    g_slave_ctx.regs.checksum = i2c_regs_calculate_checksum(&g_slave_ctx.regs);
}

void i2c_sched_slave_update_error(uint8_t error_bit, bool set) {
    if (set) {
        g_slave_ctx.regs.error |= error_bit;
        g_slave_ctx.pending_event = SYNC_EVENT_ERROR;
    } else {
        g_slave_ctx.regs.error &= ~error_bit;
    }
    g_slave_ctx.regs.checksum = i2c_regs_calculate_checksum(&g_slave_ctx.regs);
}

void i2c_sched_slave_clear_error(void) {
    g_slave_ctx.regs.error = ERR_NONE;
    g_slave_ctx.regs.checksum = i2c_regs_calculate_checksum(&g_slave_ctx.regs);
}

void i2c_sched_slave_set_data_ready(void) {
    g_slave_ctx.regs.status |= STATUS_DATA_READY;
    g_slave_ctx.pending_event = SYNC_EVENT_DATA_READY;
    g_slave_ctx.regs.checksum = i2c_regs_calculate_checksum(&g_slave_ctx.regs);
}

void i2c_sched_slave_set_sync_locked(bool locked) {
    g_slave_ctx.sync_ctx.sync_locked = locked;
    if (locked) {
        g_slave_ctx.regs.status |= STATUS_SYNC_LOCKED;
    } else {
        g_slave_ctx.regs.status &= ~STATUS_SYNC_LOCKED;
    }
    g_slave_ctx.regs.checksum = i2c_regs_calculate_checksum(&g_slave_ctx.regs);
}

slave_state_t i2c_sched_slave_get_state(void) {
    return g_slave_ctx.state;
}

sync_event_t i2c_sched_slave_get_event(void) {
    return g_slave_ctx.pending_event;
}

void i2c_sched_slave_clear_event(void) {
    g_slave_ctx.pending_event = SYNC_EVENT_NONE;
}

bool i2c_sched_slave_regs_changed(void) {
    bool changed = g_slave_ctx.regs_dirty;
    g_slave_ctx.regs_dirty = false;
    return changed;
}

bool i2c_sched_slave_set_address(uint8_t i2c_addr) {
    if (!g_slave_ctx.config.i2c_port) return false;
    
    if (i2c_addr < I2C_ADDR_BASE || i2c_addr > I2C_ADDR_MAX) {
        return false;
    }
    
    if (g_slave_ctx.state != SLAVE_UNASSIGNED) {
        return false;
    }
    
    uint8_t node_id = I2C_ADDR_TO_SLOT(i2c_addr);
    g_slave_ctx.regs.node_id = node_id;
    g_slave_ctx.regs.cluster_id = 0;
    
    i2c_slave_init(g_slave_ctx.config.i2c_port, i2c_addr, i2c_slave_handler);
    
    g_slave_ctx.state = SLAVE_READY;
    g_slave_ctx.regs.checksum = i2c_regs_calculate_checksum(&g_slave_ctx.regs);
    
    return true;
}

uint8_t i2c_sched_slave_get_address(void) {
    if (g_slave_ctx.state == SLAVE_UNASSIGNED) {
        return I2C_ADDR_UNASSIGNED;
    }
    return I2C_ADDR_PICO(g_slave_ctx.regs.node_id);
}

bool i2c_sched_slave_is_assigned(void) {
    return g_slave_ctx.state != SLAVE_UNASSIGNED && 
           g_slave_ctx.state != SLAVE_IDLE &&
           g_slave_ctx.state != SLAVE_INITIALIZING;
}

void i2c_sched_slave_clear_address(void) {
    if (!g_slave_ctx.config.i2c_port) return;
    
    i2c_slave_deinit(g_slave_ctx.config.i2c_port);
    
    g_slave_ctx.state = SLAVE_UNASSIGNED;
    g_slave_ctx.regs.node_id = 0xFF;
    g_slave_ctx.regs.cluster_id = 0xFF;
    g_slave_ctx.regs.checksum = i2c_regs_calculate_checksum(&g_slave_ctx.regs);
}

void i2c_sched_slave_deinit(void) {
    if (g_slave_ctx.config.i2c_port) {
        i2c_slave_deinit(g_slave_ctx.config.i2c_port);
        i2c_deinit(g_slave_ctx.config.i2c_port);
    }
    
    memset(&g_slave_ctx, 0, sizeof(i2c_slave_ctx_t));
    g_sync_callback = NULL;
}
