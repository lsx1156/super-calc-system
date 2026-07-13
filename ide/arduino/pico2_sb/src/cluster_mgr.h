#ifndef CLUSTER_MGR_H
#define CLUSTER_MGR_H

#include <Arduino.h>
#include "config.h"

typedef struct {
    uint8_t node_id;
    bool online;
    uint8_t status;
} cluster_node_t;

typedef struct {
    uint8_t total_nodes;
    uint8_t online_count;
    uint8_t fault_count;
    cluster_node_t nodes[MAX_PICO_SLAVES];
} cluster_state_t;

void cluster_init(void);
cluster_state_t* cluster_get_state(void);
void cluster_detect_nodes(void);
void cluster_reset_node(uint8_t node_id);

#endif