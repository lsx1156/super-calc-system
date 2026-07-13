#include "usb_cdc_comm.h"
#include "tusb.h"
#include <string.h>
#include "system_status.h"
#include "cluster_mgr.h"

static usb_cmd_t g_cmd;
static uint8_t g_rx_buf[USB_RX_BUFFER_SIZE];
static uint32_t g_rx_len = 0;
static uint8_t g_node_id = 0;

static uint32_t crc32(const uint8_t *data, uint32_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return ~crc;
}

void usb_cdc_init(void) {
    tusb_init();
    g_cmd.cmd_ready = false;
    g_rx_len = 0;
    g_node_id = 0;
}

static bool parse_frame(const uint8_t* data, uint32_t len) {
    if (len < 13) return false;
    if (data[0] != FRAME_HEADER_USB) return false;
    if (data[len - 1] != FRAME_TAIL_USB) return false;
    
    g_node_id = data[1];
    
    uint16_t data_len = data[2] | (data[3] << 8);
    if (8 + data_len + 4 + 1 > len) return false;
    
    uint32_t crc_rx = data[8 + data_len] 
                    | (data[9 + data_len] << 8)
                    | (data[10 + data_len] << 16)
                    | (data[11 + data_len] << 24);
    
    uint32_t crc_calc = crc32(&data[4], 4 + data_len);
    if (crc_rx != crc_calc) return false;
    
    g_cmd.cmd = data[8];
    g_cmd.param_len = data_len - 1;
    if (g_cmd.param_len > 0 && g_cmd.param_len < 256) {
        memcpy(g_cmd.params, &data[9], g_cmd.param_len);
    }
    g_cmd.cmd_ready = true;
    
    return true;
}

void usb_cdc_task(void* pvParameters) {
    while (1) {
        tud_task();
        
        if (tud_cdc_available()) {
            uint32_t count = tud_cdc_read(&g_rx_buf[g_rx_len], USB_RX_BUFFER_SIZE - g_rx_len);
            if (count > 0) {
                g_rx_len += count;
                
                uint32_t start = 0;
                while (start < g_rx_len) {
                    if (g_rx_buf[start] != FRAME_HEADER_USB) {
                        start++;
                        continue;
                    }
                    
                    uint32_t tail_pos = start;
                    while (tail_pos < g_rx_len && g_rx_buf[tail_pos] != FRAME_TAIL_USB) {
                        tail_pos++;
                    }
                    
                    if (tail_pos < g_rx_len) {
                        if (parse_frame(&g_rx_buf[start], tail_pos - start + 1)) {
                            start = tail_pos + 1;
                            continue;
                        }
                    }
                    break;
                }
                
                if (start > 0 && start < g_rx_len) {
                    memmove(g_rx_buf, &g_rx_buf[start], g_rx_len - start);
                    g_rx_len -= start;
                } else if (start == g_rx_len) {
                    g_rx_len = 0;
                }
                
                if (g_rx_len >= USB_RX_BUFFER_SIZE) {
                    g_rx_len = 0;
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(USB_PERIOD_MS));
    }
}

bool usb_cdc_get_cmd(usb_cmd_t* cmd) {
    if (!g_cmd.cmd_ready) return false;
    memcpy(cmd, &g_cmd, sizeof(usb_cmd_t));
    g_cmd.cmd_ready = false;
    return true;
}

void usb_cdc_send_data(const uint8_t* data, uint16_t len) {
    if (!tud_cdc_connected()) return;
    
    static uint8_t frame[USB_TX_BUFFER_SIZE];
    uint32_t timestamp = time_us_32();
    
    frame[0] = FRAME_HEADER_USB;
    frame[1] = g_node_id;
    frame[2] = len & 0xFF;
    frame[3] = (len >> 8) & 0xFF;
    
    frame[4] = timestamp & 0xFF;
    frame[5] = (timestamp >> 8) & 0xFF;
    frame[6] = (timestamp >> 16) & 0xFF;
    frame[7] = (timestamp >> 24) & 0xFF;
    
    memcpy(&frame[8], data, len);
    
    uint32_t crc = crc32(&frame[4], 4 + len);
    frame[8 + len] = crc & 0xFF;
    frame[9 + len] = (crc >> 8) & 0xFF;
    frame[10 + len] = (crc >> 16) & 0xFF;
    frame[11 + len] = (crc >> 24) & 0xFF;
    frame[12 + len] = FRAME_TAIL_USB;
    
    uint32_t frame_len = 13 + len;
    
    tud_cdc_write(frame, frame_len);
    tud_cdc_write_flush();
}

void usb_cdc_send_status(void) {
    system_status_t* st = system_status_get();
    cluster_state_t* cluster = cluster_get_state();
    
    uint8_t buf[512];
    uint16_t offset = 0;
    
    buf[offset++] = DATA_STATUS;
    memcpy(&buf[offset], st, sizeof(system_status_t));
    offset += sizeof(system_status_t);
    
    buf[offset++] = cluster->total_nodes;
    buf[offset++] = cluster->online_count;
    buf[offset++] = cluster->fault_count;
    
    usb_cdc_send_data(buf, offset);
}
