#ifndef _ADDR_ASSIGN_PROTOCOL_H
#define _ADDR_ASSIGN_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_SLOT_COUNT      8
#define I2C_ADDR_UNASSIGNED 0xFF

#define SLOT_TO_I2C_ADDR(slot) (0x40 + (slot))

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

#endif