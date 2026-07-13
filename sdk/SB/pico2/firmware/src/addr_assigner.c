#include "addr_assigner.h"
#include "spi_master.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "config.h"
#include <string.h>

static addr_assigner_ctx_t g_assigner_ctx;

void addr_assigner_init(void) {
    memset(&g_assigner_ctx, 0, sizeof(g_assigner_ctx));
    g_assigner_ctx.state = ASSIGNER_IDLE;
    
    for (int i = 0; i < MAX_SLOT_COUNT; i++) {
        g_assigner_ctx.slots[i].slot_id = i;
        g_assigner_ctx.slots[i].i2c_addr = I2C_ADDR_UNASSIGNED;
        g_assigner_ctx.slots[i].node_state = NODE_STATE_UNASSIGNED;
        g_assigner_ctx.slots[i].online = false;
        g_assigner_ctx.slots[i].firmware_ver = 0;
    }
}

bool addr_assigner_assign_slot(uint8_t slot_id) {
    if (slot_id >= MAX_SLOT_COUNT) return false;
    
    uint8_t params[1] = {slot_id};
    uint8_t resp[128];
    uint16_t resp_len = 0;
    
    if (!spi_master_send_cmd(slot_id, CMD_ADDR_ASSIGN, params, 1, resp, &resp_len)) {
        g_assigner_ctx.slots[slot_id].online = false;
        g_assigner_ctx.slots[slot_id].node_state = NODE_STATE_UNASSIGNED;
        return false;
    }
    
    if (resp_len < 1) {
        g_assigner_ctx.slots[slot_id].online = false;
        return false;
    }
    
    uint8_t ack_code = resp[0];
    
    if (ack_code == 0x06 || ack_code == 0x10) {
        uint8_t i2c_addr = (resp_len >= 2) ? resp[1] : SLOT_TO_I2C_ADDR(slot_id);
        g_assigner_ctx.slots[slot_id].i2c_addr = i2c_addr;
        g_assigner_ctx.slots[slot_id].node_state = NODE_STATE_ASSIGNED;
        g_assigner_ctx.slots[slot_id].online = true;
        return true;
    }
    
    g_assigner_ctx.slots[slot_id].online = false;
    return false;
}

uint8_t addr_assigner_scan_all(void) {
    g_assigner_ctx.state = ASSIGNER_SCANNING;
    g_assigner_ctx.scan_start_time = time_us_32();
    g_assigner_ctx.assigned_count = 0;
    g_assigner_ctx.total_detected = 0;
    
    for (int i = 0; i < MAX_SLOT_COUNT; i++) {
        g_assigner_ctx.slots[i].online = false;
        g_assigner_ctx.slots[i].i2c_addr = I2C_ADDR_UNASSIGNED;
        g_assigner_ctx.slots[i].node_state = NODE_STATE_UNASSIGNED;
    }
    
    for (int i = 0; i < MAX_SLOT_COUNT; i++) {
        if (addr_assigner_assign_slot(i)) {
            g_assigner_ctx.assigned_count++;
            g_assigner_ctx.total_detected++;
        }
    }
    
    g_assigner_ctx.state = ASSIGNER_COMPLETE;
    g_assigner_ctx.scan_duration_ms = (time_us_32() - g_assigner_ctx.scan_start_time) / 1000;
    
    return g_assigner_ctx.assigned_count;
}

bool addr_assigner_query_slot(uint8_t slot_id, slot_mapping_t* mapping) {
    if (slot_id >= MAX_SLOT_COUNT) return false;
    
    uint8_t resp[128];
    uint16_t resp_len = 0;
    
    if (!spi_master_send_cmd(slot_id, CMD_ADDR_QUERY, NULL, 0, resp, &resp_len)) {
        g_assigner_ctx.slots[slot_id].online = false;
        if (mapping) *mapping = g_assigner_ctx.slots[slot_id];
        return false;
    }
    
    if (resp_len < 1 || resp[0] != 0x06) {
        g_assigner_ctx.slots[slot_id].online = false;
        if (mapping) *mapping = g_assigner_ctx.slots[slot_id];
        return false;
    }
    
    g_assigner_ctx.slots[slot_id].online = true;
    if (resp_len >= 2) {
        g_assigner_ctx.slots[slot_id].i2c_addr = resp[1];
    }
    g_assigner_ctx.slots[slot_id].node_state = NODE_STATE_ASSIGNED;
    
    if (mapping) {
        *mapping = g_assigner_ctx.slots[slot_id];
    }
    
    return true;
}

bool addr_assigner_clear_slot(uint8_t slot_id) {
    if (slot_id >= MAX_SLOT_COUNT) return false;
    
    uint8_t resp[128];
    uint16_t resp_len = 0;
    
    spi_master_send_cmd(slot_id, CMD_ADDR_CLEAR, NULL, 0, resp, &resp_len);
    
    g_assigner_ctx.slots[slot_id].i2c_addr = I2C_ADDR_UNASSIGNED;
    g_assigner_ctx.slots[slot_id].node_state = NODE_STATE_UNASSIGNED;
    g_assigner_ctx.slots[slot_id].online = false;
    
    if (g_assigner_ctx.assigned_count > 0) {
        g_assigner_ctx.assigned_count--;
    }
    
    return true;
}

const slot_mapping_t* addr_assigner_get_mapping(void) {
    return g_assigner_ctx.slots;
}

uint8_t addr_assigner_get_assigned_count(void) {
    return g_assigner_ctx.assigned_count;
}

assigner_state_t addr_assigner_get_state(void) {
    return g_assigner_ctx.state;
}

uint8_t addr_assigner_rescan_unassigned(void) {
    uint8_t newly_assigned = 0;
    
    for (int i = 0; i < MAX_SLOT_COUNT; i++) {
        if (!g_assigner_ctx.slots[i].online || 
            g_assigner_ctx.slots[i].node_state == NODE_STATE_UNASSIGNED) {
            if (addr_assigner_assign_slot(i)) {
                newly_assigned++;
            }
        }
    }
    
    g_assigner_ctx.assigned_count += newly_assigned;
    return newly_assigned;
}

bool addr_assigner_all_assigned(void) {
    for (int i = 0; i < MAX_SLOT_COUNT; i++) {
        if (g_assigner_ctx.slots[i].online && 
            g_assigner_ctx.slots[i].node_state != NODE_STATE_ASSIGNED) {
            return false;
        }
    }
    return true;
}
