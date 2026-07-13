#ifndef ADDR_ASSIGNER_H
#define ADDR_ASSIGNER_H

#include <Arduino.h>

#define MAX_SLOT_COUNT      8
#define I2C_ADDR_UNASSIGNED 0xFF

#define SLOT_TO_I2C_ADDR(slot) (I2C_SCHED_ADDR_BASE + (slot))

typedef enum {
    NODE_STATE_UNASSIGNED = 0,
    NODE_STATE_ASSIGNED = 1,
    NODE_STATE_ERROR = 2
} node_state_t;

typedef struct {
    uint8_t slot_id;
    uint8_t i2c_addr;
    node_state_t node_state;
    bool online;
    uint8_t firmware_ver;
} slot_mapping_t;

typedef enum {
    ASSIGNER_IDLE = 0,
    ASSIGNER_SCANNING,
    ASSIGNER_COMPLETE,
    ASSIGNER_ERROR
} assigner_state_t;

typedef struct {
    assigner_state_t state;
    slot_mapping_t slots[MAX_SLOT_COUNT];
    uint8_t assigned_count;
    uint8_t total_detected;
    uint32_t scan_start_time;
    uint32_t scan_duration_ms;
} addr_assigner_ctx_t;

void addr_assigner_init(void);
uint8_t addr_assigner_scan_all(void);
bool addr_assigner_assign_slot(uint8_t slot_id);
bool addr_assigner_query_slot(uint8_t slot_id, slot_mapping_t* mapping);
bool addr_assigner_clear_slot(uint8_t slot_id);
const slot_mapping_t* addr_assigner_get_mapping(void);
uint8_t addr_assigner_get_assigned_count(void);
assigner_state_t addr_assigner_get_state(void);
uint8_t addr_assigner_rescan_unassigned(void);
bool addr_assigner_all_assigned(void);

#endif