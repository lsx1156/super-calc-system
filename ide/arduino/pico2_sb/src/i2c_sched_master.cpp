#include "i2c_sched_master.h"
#include <string.h>

static i2c_sched_ctx_t g_sched_ctx;

i2c_status_t i2c_sched_master_init(const i2c_master_config_t* config) {
    memset(&g_sched_ctx, 0, sizeof(i2c_sched_ctx_t));
    
    if (!config || !config->i2c_port) {
        return I2C_ERR_INVALID_ADDR;
    }
    
    g_sched_ctx.config = *config;
    g_sched_ctx.state = SCHED_INITIALIZING;
    g_sched_ctx.node_count = 0;
    g_sched_ctx.sync_interval_us = 1000;
    
    config->i2c_port->begin(config->sda_pin, config->scl_pin, config->bus_freq);
    
    for (int i = 0; i < MAX_PICO_SLAVES; i++) {
        g_sched_ctx.nodes[i].node_id = i;
        g_sched_ctx.nodes[i].online = false;
    }
    
    g_sched_ctx.state = SCHED_READY;
    return I2C_OK;
}

i2c_status_t i2c_sched_read_reg(uint8_t node_id, uint8_t reg_addr, uint8_t* value) {
    if (node_id >= MAX_PICO_SLAVES) {
        return I2C_ERR_INVALID_ADDR;
    }
    
    uint8_t addr = I2C_SCHED_ADDR_BASE + node_id;
    
    g_sched_ctx.config.i2c_port->beginTransmission(addr);
    g_sched_ctx.config.i2c_port->write(reg_addr);
    int ret = g_sched_ctx.config.i2c_port->endTransmission(true);
    
    if (ret != 0) {
        return I2C_ERR_NACK;
    }
    
    ret = g_sched_ctx.config.i2c_port->requestFrom((int)addr, 1);
    if (ret != 1) {
        return I2C_ERR_NACK;
    }
    
    *value = g_sched_ctx.config.i2c_port->read();
    return I2C_OK;
}

i2c_status_t i2c_sched_write_reg(uint8_t node_id, uint8_t reg_addr, uint8_t value) {
    if (node_id >= MAX_PICO_SLAVES) {
        return I2C_ERR_INVALID_ADDR;
    }
    
    uint8_t addr = I2C_SCHED_ADDR_BASE + node_id;
    
    g_sched_ctx.config.i2c_port->beginTransmission(addr);
    g_sched_ctx.config.i2c_port->write(reg_addr);
    g_sched_ctx.config.i2c_port->write(value);
    int ret = g_sched_ctx.config.i2c_port->endTransmission(false);
    
    if (ret != 0) {
        return I2C_ERR_NACK;
    }
    
    return I2C_OK;
}

i2c_status_t i2c_sched_read_regs(uint8_t node_id, uint8_t start_addr, 
                                  uint8_t* data, uint8_t count) {
    if (node_id >= MAX_PICO_SLAVES) {
        return I2C_ERR_INVALID_ADDR;
    }
    
    uint8_t addr = I2C_SCHED_ADDR_BASE + node_id;
    
    g_sched_ctx.config.i2c_port->beginTransmission(addr);
    g_sched_ctx.config.i2c_port->write(start_addr);
    int ret = g_sched_ctx.config.i2c_port->endTransmission(true);
    
    if (ret != 0) {
        return I2C_ERR_NACK;
    }
    
    ret = g_sched_ctx.config.i2c_port->requestFrom((int)addr, (int)count);
    if (ret != count) {
        return I2C_ERR_NACK;
    }
    
    for (int i = 0; i < count; i++) {
        data[i] = g_sched_ctx.config.i2c_port->read();
    }
    
    return I2C_OK;
}

i2c_status_t i2c_sched_write_regs(uint8_t node_id, uint8_t start_addr, 
                                   const uint8_t* data, uint8_t count) {
    if (node_id >= MAX_PICO_SLAVES) {
        return I2C_ERR_INVALID_ADDR;
    }
    
    uint8_t addr = I2C_SCHED_ADDR_BASE + node_id;
    
    g_sched_ctx.config.i2c_port->beginTransmission(addr);
    g_sched_ctx.config.i2c_port->write(start_addr);
    for (int i = 0; i < count; i++) {
        g_sched_ctx.config.i2c_port->write(data[i]);
    }
    int ret = g_sched_ctx.config.i2c_port->endTransmission(false);
    
    if (ret != 0) {
        return I2C_ERR_NACK;
    }
    
    return I2C_OK;
}

i2c_status_t i2c_sched_broadcast_write(uint8_t reg_addr, uint8_t value) {
    g_sched_ctx.config.i2c_port->beginTransmission(I2C_SCHED_ADDR_BASE - 1);
    g_sched_ctx.config.i2c_port->write(reg_addr);
    g_sched_ctx.config.i2c_port->write(value);
    int ret = g_sched_ctx.config.i2c_port->endTransmission(false);
    
    if (ret != 0) {
        return I2C_ERR_NACK;
    }
    
    return I2C_OK;
}

i2c_status_t i2c_sched_broadcast_write_regs(uint8_t start_addr, 
                                             const uint8_t* data, uint8_t count) {
    g_sched_ctx.config.i2c_port->beginTransmission(I2C_SCHED_ADDR_BASE - 1);
    g_sched_ctx.config.i2c_port->write(start_addr);
    for (int i = 0; i < count; i++) {
        g_sched_ctx.config.i2c_port->write(data[i]);
    }
    int ret = g_sched_ctx.config.i2c_port->endTransmission(false);
    
    if (ret != 0) {
        return I2C_ERR_NACK;
    }
    
    return I2C_OK;
}

i2c_status_t i2c_sched_trigger_sync(void) {
    uint32_t sync_time = micros();
    
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
        uint8_t addr = I2C_SCHED_ADDR_BASE + i;
        
        g_sched_ctx.config.i2c_port->beginTransmission(addr);
        int ret = g_sched_ctx.config.i2c_port->endTransmission(true);
        
        if (ret == 0) {
            g_sched_ctx.nodes[i].online = true;
            g_sched_ctx.node_count++;
            
            uint8_t status, version, node_id, cluster_id;
            i2c_sched_read_reg(i, REG_STATUS, &status);
            i2c_sched_read_reg(i, REG_VERSION, &version);
            i2c_sched_read_reg(i, REG_NODE_ID, &node_id);
            i2c_sched_read_reg(i, REG_CLUSTER_ID, &cluster_id);
            
            g_sched_ctx.nodes[i].status = status;
            g_sched_ctx.nodes[i].version = version;
            g_sched_ctx.nodes[i].node_id = node_id;
            g_sched_ctx.nodes[i].cluster_id = cluster_id;
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
    
    uint32_t now = micros();
    
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
        g_sched_ctx.config.i2c_port->end();
    }
    
    memset(&g_sched_ctx, 0, sizeof(i2c_sched_ctx_t));
}