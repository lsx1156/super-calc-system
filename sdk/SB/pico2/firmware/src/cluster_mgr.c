#include "cluster_mgr.h"
#include "spi_master.h"
#include <string.h>
#include "system_status.h"

static cluster_state_t g_cluster;

void cluster_init(void) {
    memset(&g_cluster, 0, sizeof(g_cluster));
    g_cluster.total_nodes = DEFAULT_PICO_COUNT;
    
    for (int i = 0; i < MAX_PICO_SLAVES; i++) {
        g_cluster.nodes[i].online = false;
        g_cluster.nodes[i].fault = false;
        g_cluster.nodes[i].error_count = 0;
        g_cluster.nodes[i].last_comm_ms = 0;
    }
}

bool cluster_detect_nodes(void) {
    uint8_t resp[64];
    uint16_t resp_len = 0;
    uint8_t online = 0;
    
    for (int i = 0; i < g_cluster.total_nodes; i++) {
        bool success = spi_master_send_cmd(i, CMD_GET_STATUS, NULL, 0, resp, &resp_len);
        if (success) {
            g_cluster.nodes[i].online = true;
            g_cluster.nodes[i].fault = false;
            g_cluster.nodes[i].last_comm_ms = 0;
            online++;
        } else {
            g_cluster.nodes[i].online = false;
        }
    }
    
    g_cluster.online_count = online;
    system_status_t* st = system_status_get();
    st->online_count = online;
    
    return online > 0;
}

uint8_t cluster_get_online_count(void) {
    return g_cluster.online_count;
}

uint8_t cluster_get_fault_count(void) {
    return g_cluster.fault_count;
}

bool cluster_is_node_online(uint8_t node_id) {
    if (node_id >= MAX_PICO_SLAVES) return false;
    return g_cluster.nodes[node_id].online;
}

bool cluster_reset_node(uint8_t node_id) {
    if (node_id >= MAX_PICO_SLAVES) return false;
    
    uint8_t resp[16];
    uint16_t resp_len = 0;
    
    bool success = spi_master_send_cmd(node_id, CMD_RESET, NULL, 0, resp, &resp_len);
    
    g_cluster.nodes[node_id].online = false;
    g_cluster.nodes[node_id].fault = false;
    g_cluster.nodes[node_id].error_count = 0;
    
    return success;
}

void cluster_report_error(uint8_t node_id) {
    if (node_id >= MAX_PICO_SLAVES) return;
    
    g_cluster.nodes[node_id].error_count++;
    
    if (g_cluster.nodes[node_id].error_count >= MAX_ERROR_COUNT) {
        g_cluster.nodes[node_id].fault = true;
        g_cluster.nodes[node_id].online = false;
        g_cluster.fault_count++;
    }
}

cluster_state_t* cluster_get_state(void) {
    return &g_cluster;
}

void cluster_update_node_status(uint8_t node_id, uint8_t mode, uint8_t running, float temp) {
    if (node_id >= MAX_PICO_SLAVES) return;
    
    g_cluster.nodes[node_id].work_mode = mode;
    g_cluster.nodes[node_id].run_status = running;
    g_cluster.nodes[node_id].temperature = temp;
    g_cluster.nodes[node_id].last_comm_ms = 0;
    
    if (temp >= TEMP_SHUTDOWN) {
        g_cluster.nodes[node_id].fault = true;
        g_cluster.nodes[node_id].fault_code = 2;
    }
}

bool cluster_auto_heal(void) {
    bool healed = false;
    
    for (int i = 0; i < g_cluster.total_nodes; i++) {
        if (g_cluster.nodes[i].fault && !g_cluster.nodes[i].online) {
            cluster_reset_node(i);
            vTaskDelay(pdMS_TO_TICKS(100));
            
            uint8_t resp[64];
            uint16_t resp_len = 0;
            if (spi_master_send_cmd(i, CMD_GET_STATUS, NULL, 0, resp, &resp_len)) {
                g_cluster.nodes[i].online = true;
                g_cluster.nodes[i].fault = false;
                g_cluster.nodes[i].error_count = 0;
                g_cluster.fault_count--;
                healed = true;
            }
        }
    }
    
    if (healed) {
        uint8_t online = 0;
        for (int i = 0; i < g_cluster.total_nodes; i++) {
            if (g_cluster.nodes[i].online) online++;
        }
        g_cluster.online_count = online;
        system_status_t* st = system_status_get();
        st->online_count = online;
    }
    
    return healed;
}
