#include "cluster_mgr.h"
#include "spi_master.h"

static cluster_state_t g_cluster;

void cluster_init(void) {
    memset(&g_cluster, 0, sizeof(g_cluster));
    g_cluster.total_nodes = DEFAULT_PICO_COUNT;
}

cluster_state_t* cluster_get_state(void) {
    return &g_cluster;
}

void cluster_detect_nodes(void) {
    g_cluster.online_count = 0;
    g_cluster.fault_count = 0;
    
    for (int i = 0; i < g_cluster.total_nodes; i++) {
        uint8_t resp[64];
        uint16_t resp_len = 0;
        
        if (spi_master_send_cmd(i, CMD_GET_STATUS, NULL, 0, resp, &resp_len)) {
            g_cluster.nodes[i].online = true;
            g_cluster.online_count++;
        } else {
            g_cluster.nodes[i].online = false;
            g_cluster.fault_count++;
        }
        delayMicroseconds(1);
    }
}

void cluster_reset_node(uint8_t node_id) {
    if (node_id < g_cluster.total_nodes) {
        spi_master_send_cmd(node_id, CMD_RESET, NULL, 0, NULL, NULL);
        g_cluster.nodes[node_id].online = false;
    }
}