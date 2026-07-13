#ifndef _CLUSTER_MGR_H
#define _CLUSTER_MGR_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

typedef struct {
    bool     online;
    bool     fault;
    uint8_t  fault_code;
    uint32_t error_count;
    uint32_t last_comm_ms;
    uint8_t  work_mode;
    uint8_t  run_status;
    float    temperature;
    uint32_t overclock_freq;
    uint32_t sample_count;
} node_info_t;

typedef struct {
    uint8_t     total_nodes;
    uint8_t     online_count;
    uint8_t     fault_count;
    uint8_t     active_count;
    node_info_t nodes[MAX_PICO_SLAVES];
} cluster_state_t;

void cluster_init(void);
bool cluster_detect_nodes(void);
uint8_t cluster_get_online_count(void);
uint8_t cluster_get_fault_count(void);
bool cluster_is_node_online(uint8_t node_id);
bool cluster_reset_node(uint8_t node_id);
void cluster_report_error(uint8_t node_id);
cluster_state_t* cluster_get_state(void);
void cluster_update_node_status(uint8_t node_id, uint8_t mode, uint8_t running, float temp);
bool cluster_auto_heal(void);

#endif
