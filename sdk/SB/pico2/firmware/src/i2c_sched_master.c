#include "i2c_sched_master.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <string.h>

static i2c_sched_ctx_t g_sched_ctx;

uint8_t i2c_regs_calculate_checksum(const i2c_regs_t* regs) {
    uint8_t sum = 0;
    const uint8_t* ptr = (const uint8_t*)regs;
    
    for (int i = 0; i < REG_CHECKSUM; i++) {
        sum ^= ptr[i];
    }
    for (int i = REG_CHECKSUM + 1; i < sizeof(i2c_regs_t); i++) {
        sum ^= ptr[i];
    }
    
    return sum;
}

bool i2c_regs_validate_checksum(const i2c_regs_t* regs) {
    return regs->checksum == i2c_regs_calculate_checksum(regs);
}

void i2c_regs_reset(i2c_regs_t* regs) {
    memset(regs, 0, sizeof(i2c_regs_t));
    regs->ctrl = CTRL_STOP_MASK;
    regs->mode = MODE_SAMPLE;
    regs->sample_rate = RATE_50KHZ;
    regs->adc_ch_sel = ADC_ALL_CH;
    regs->dig_ch_sel = DIG_ALL_CH;
    regs->checksum = i2c_regs_calculate_checksum(regs);
}

void i2c_regs_init(i2c_regs_t* regs, uint8_t node_id, uint8_t cluster_id) {
    i2c_regs_reset(regs);
    regs->node_id = node_id;
    regs->cluster_id = cluster_id;
    regs->version = (FW_VERSION_MAJOR << FW_VER_MAJOR_SHIFT) | FW_VERSION_MINOR;
    regs->checksum = i2c_regs_calculate_checksum(regs);
}

i2c_status_t i2c_sched_master_init(const i2c_master_config_t* config) {
    memset(&g_sched_ctx, 0, sizeof(i2c_sched_ctx_t));
    
    if (!config || !config->i2c_port) {
        return I2C_ERR_INVALID_ADDR;
    }
    
    g_sched_ctx.config = *config;
    g_sched_ctx.state = SCHED_INITIALIZING;
    g_sched_ctx.node_count = 0;
    g_sched_ctx.sync_interval_us = 1000;
    
    i2c_init(config->i2c_port, config->bus_freq);
    
    gpio_set_function(config->sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(config->scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(config->sda_pin);
    gpio_pull_up(config->scl_pin);
    
    for (int i = 0; i < MAX_PICO_SLAVES; i++) {
        g_sched_ctx.nodes[i].node_id = i;
        g_sched_ctx.nodes[i].online = false;
        i2c_regs_reset(&g_sched_ctx.nodes[i].cached_regs);
    }
    
    g_sched_ctx.state = SCHED_READY;
    return I2C_OK;
}

i2c_status_t i2c_sched_read_reg(uint8_t node_id, uint8_t reg_addr, uint8_t* value) {
    if (node_id >= MAX_PICO_SLAVES) {
        return I2C_ERR_INVALID_ADDR;
    }
    
    if (reg_addr > REG_RESERVED_END) {
        return I2C_ERR_INVALID_REG;
    }
    
    uint8_t addr = I2C_ADDR_PICO(node_id);
    int ret;
    
    ret = i2c_write_blocking(g_sched_ctx.config.i2c_port, addr, &reg_addr, 1, true);
    if (ret < 0) {
        return I2C_ERR_NACK;
    }
    
    ret = i2c_read_blocking(g_sched_ctx.config.i2c_port, addr, value, 1, false);
    if (ret < 0) {
        return I2C_ERR_NACK;
    }
    
    return I2C_OK;
}

i2c_status_t i2c_sched_write_reg(uint8_t node_id, uint8_t reg_addr, uint8_t value) {
    if (node_id >= MAX_PICO_SLAVES) {
        return I2C_ERR_INVALID_ADDR;
    }
    
    if (reg_addr > REG_RESERVED_END) {
        return I2C_ERR_INVALID_REG;
    }
    
    uint8_t addr = I2C_ADDR_PICO(node_id);
    uint8_t data[2] = {reg_addr, value};
    
    int ret = i2c_write_blocking(g_sched_ctx.config.i2c_port, addr, data, sizeof(data), false);
    if (ret < 0) {
        return I2C_ERR_NACK;
    }
    
    if (reg_addr != REG_CHECKSUM && reg_addr != REG_NODE_ID && reg_addr != REG_CLUSTER_ID) {
        g_sched_ctx.nodes[node_id].cached_regs.checksum = 0;
    }
    
    return I2C_OK;
}

i2c_status_t i2c_sched_read_regs(uint8_t node_id, uint8_t start_addr, 
                                  uint8_t* data, uint8_t count) {
    if (node_id >= MAX_PICO_SLAVES) {
        return I2C_ERR_INVALID_ADDR;
    }
    
    if (start_addr + count > REG_COUNT) {
        return I2C_ERR_INVALID_REG;
    }
    
    uint8_t addr = I2C_ADDR_PICO(node_id);
    int ret;
    
    ret = i2c_write_blocking(g_sched_ctx.config.i2c_port, addr, &start_addr, 1, true);
    if (ret < 0) {
        return I2C_ERR_NACK;
    }
    
    ret = i2c_read_blocking(g_sched_ctx.config.i2c_port, addr, data, count, false);
    if (ret < 0) {
        return I2C_ERR_NACK;
    }
    
    return I2C_OK;
}

i2c_status_t i2c_sched_write_regs(uint8_t node_id, uint8_t start_addr, 
                                   const uint8_t* data, uint8_t count) {
    if (node_id >= MAX_PICO_SLAVES) {
        return I2C_ERR_INVALID_ADDR;
    }
    
    if (start_addr + count > REG_COUNT) {
        return I2C_ERR_INVALID_REG;
    }
    
    uint8_t addr = I2C_ADDR_PICO(node_id);
    uint8_t buf[REG_COUNT + 1];
    
    if (count + 1 > sizeof(buf)) {
        return I2C_ERR_DATA_OVERFLOW;
    }
    
    buf[0] = start_addr;
    memcpy(&buf[1], data, count);
    
    int ret = i2c_write_blocking(g_sched_ctx.config.i2c_port, addr, buf, count + 1, false);
    if (ret < 0) {
        return I2C_ERR_NACK;
    }
    
    g_sched_ctx.nodes[node_id].cached_regs.checksum = 0;
    
    return I2C_OK;
}

i2c_status_t i2c_sched_broadcast_write(uint8_t reg_addr, uint8_t value) {
    if (reg_addr > REG_RESERVED_END) {
        return I2C_ERR_INVALID_REG;
    }
    
    uint8_t data[2] = {reg_addr, value};
    
    int ret = i2c_write_blocking(g_sched_ctx.config.i2c_port, I2C_BROADCAST_ADDR, 
                                  data, sizeof(data), false);
    if (ret < 0) {
        return I2C_ERR_NACK;
    }
    
    for (int i = 0; i < MAX_PICO_SLAVES; i++) {
        if (g_sched_ctx.nodes[i].online) {
            g_sched_ctx.nodes[i].cached_regs.checksum = 0;
        }
    }
    
    return I2C_OK;
}

i2c_status_t i2c_sched_broadcast_write_regs(uint8_t start_addr, 
                                             const uint8_t* data, uint8_t count) {
    if (start_addr + count > REG_COUNT) {
        return I2C_ERR_INVALID_REG;
    }
    
    uint8_t buf[REG_COUNT + 1];
    
    if (count + 1 > sizeof(buf)) {
        return I2C_ERR_DATA_OVERFLOW;
    }
    
    buf[0] = start_addr;
    memcpy(&buf[1], data, count);
    
    int ret = i2c_write_blocking(g_sched_ctx.config.i2c_port, I2C_BROADCAST_ADDR, 
                                  buf, count + 1, false);
    if (ret < 0) {
        return I2C_ERR_NACK;
    }
    
    for (int i = 0; i < MAX_PICO_SLAVES; i++) {
        if (g_sched_ctx.nodes[i].online) {
            g_sched_ctx.nodes[i].cached_regs.checksum = 0;
        }
    }
    
    return I2C_OK;
}

i2c_status_t i2c_sched_trigger_sync(void) {
    uint32_t sync_time = time_us_32();
    
    g_sched_ctx.sync_ctx.sync_locked = true;
    g_sched_ctx.sync_ctx.sync_timestamp = sync_time;
    g_sched_ctx.sync_ctx.trigger_count++;
    g_sched_ctx.last_sync_time = sync_time;
    
    for (int i = 0; i < MAX_PICO_SLAVES; i++) {
        if (g_sched_ctx.nodes[i].online) {
            i2c_sched_write_reg(i, REG_SYNC_TIME_0, sync_time & 0xFF);
            i2c_sched_write_reg(i, REG_SYNC_TIME_1, (sync_time >> 8) & 0xFF);
            i2c_sched_write_reg(i, REG_SYNC_TIME_2, (sync_time >> 16) & 0xFF);
            i2c_sched_write_reg(i, REG_SYNC_TIME_3, (sync_time >> 24) & 0xFF);
            
            i2c_sched_write_reg(i, REG_SYNC_TRIGGER, TRIGGER_ACTIVE);
            i2c_sched_write_reg(i, REG_SYNC_TRIGGER, TRIGGER_INACTIVE);
        }
    }
    
    return I2C_OK;
}

i2c_status_t i2c_sched_detect_nodes(void) {
    g_sched_ctx.node_count = 0;
    
    for (int i = 0; i < MAX_PICO_SLAVES; i++) {
        uint8_t addr = I2C_ADDR_PICO(i);
        uint8_t dummy;
        int ret;
        
        ret = i2c_write_blocking(g_sched_ctx.config.i2c_port, addr, &dummy, 0, true);
        
        if (ret >= 0) {
            g_sched_ctx.nodes[i].online = true;
            g_sched_ctx.node_count++;
            
            i2c_sched_read_reg(i, REG_STATUS, &g_sched_ctx.nodes[i].status);
            i2c_sched_read_reg(i, REG_VERSION, &g_sched_ctx.nodes[i].version);
            i2c_sched_read_reg(i, REG_NODE_ID, &g_sched_ctx.nodes[i].node_id);
            i2c_sched_read_reg(i, REG_CLUSTER_ID, &g_sched_ctx.nodes[i].cluster_id);
        } else {
            g_sched_ctx.nodes[i].online = false;
        }
    }
    
    return I2C_OK;
}

const i2c_node_state_t* i2c_sched_get_node_state(uint8_t node_id) {
    if (node_id >= MAX_PICO_SLAVES) {
        return NULL;
    }
    return &g_sched_ctx.nodes[node_id];
}

sched_state_t i2c_sched_get_state(void) {
    return g_sched_ctx.state;
}

i2c_status_t i2c_sched_start(void) {
    if (g_sched_ctx.state != SCHED_READY) {
        return I2C_ERR_BUS_BUSY;
    }
    
    i2c_status_t ret = i2c_sched_broadcast_write(REG_CTRL, CTRL_RUN_MASK);
    
    if (ret == I2C_OK) {
        g_sched_ctx.state = SCHED_RUNNING;
    }
    
    return ret;
}

i2c_status_t i2c_sched_stop(void) {
    i2c_status_t ret = i2c_sched_broadcast_write(REG_CTRL, CTRL_STOP_MASK);
    
    if (ret == I2C_OK) {
        g_sched_ctx.state = SCHED_READY;
        g_sched_ctx.sync_ctx.sync_locked = false;
    }
    
    return ret;
}

void i2c_sched_sync_update(void) {
    if (g_sched_ctx.state != SCHED_RUNNING) {
        return;
    }
    
    uint32_t now = time_us_32();
    
    if (now - g_sched_ctx.last_sync_time >= g_sched_ctx.sync_interval_us) {
        g_sched_ctx.last_sync_time = now;
        
        for (int i = 0; i < MAX_PICO_SLAVES; i++) {
            if (g_sched_ctx.nodes[i].online) {
                uint8_t status;
                if (i2c_sched_read_reg(i, REG_STATUS, &status) == I2C_OK) {
                    g_sched_ctx.nodes[i].status = status;
                    g_sched_ctx.nodes[i].sync_locked = (status & STATUS_SYNC_LOCKED) != 0;
                }
                
                uint8_t error;
                if (i2c_sched_read_reg(i, REG_ERROR, &error) == I2C_OK) {
                    g_sched_ctx.nodes[i].error = error;
                }
            }
        }
    }
}

void i2c_sched_master_deinit(void) {
    i2c_sched_stop();
    
    if (g_sched_ctx.config.i2c_port) {
        i2c_deinit(g_sched_ctx.config.i2c_port);
    }
    
    memset(&g_sched_ctx, 0, sizeof(i2c_sched_ctx_t));
}
