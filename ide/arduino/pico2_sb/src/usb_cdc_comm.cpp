#include "usb_cdc_comm.h"

static usb_cmd_t g_cmd;
static uint8_t g_rx_buf[512];
static uint32_t g_rx_len = 0;

uint32_t crc32(const uint8_t *data, uint32_t len) {
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
    Serial.begin(USB_BAUDRATE);
    g_cmd.cmd_ready = false;
    g_rx_len = 0;
}

void usb_cdc_task(void) {
    while (Serial.available() > 0) {
        if (g_rx_len < sizeof(g_rx_buf)) {
            g_rx_buf[g_rx_len++] = Serial.read();
        }
    }
    
    if (g_rx_len >= 13) {
        if (g_rx_buf[0] == FRAME_HEADER_USB && g_rx_buf[g_rx_len - 1] == FRAME_TAIL_USB) {
            uint16_t data_len = g_rx_buf[2] | (g_rx_buf[3] << 8);
            
            if (8 + data_len + 4 + 1 <= g_rx_len) {
                uint32_t crc_rx = g_rx_buf[8 + data_len] 
                              | (g_rx_buf[9 + data_len] << 8)
                              | (g_rx_buf[10 + data_len] << 16)
                              | (g_rx_buf[11 + data_len] << 24);
                
                uint32_t crc_calc = crc32(&g_rx_buf[4], 4 + data_len);
                if (crc_rx == crc_calc) {
                    g_cmd.cmd = g_rx_buf[8];
                    g_cmd.param_len = data_len - 1;
                    if (g_cmd.param_len > 0 && g_cmd.param_len < 32) {
                        memcpy(g_cmd.params, &g_rx_buf[9], g_cmd.param_len);
                    }
                    g_cmd.cmd_ready = true;
                }
            }
        }
        g_rx_len = 0;
    }
}

bool usb_cdc_get_cmd(usb_cmd_t* cmd) {
    if (!g_cmd.cmd_ready) return false;
    memcpy(cmd, &g_cmd, sizeof(usb_cmd_t));
    g_cmd.cmd_ready = false;
    return true;
}

void usb_cdc_send_data(const uint8_t* data, uint16_t len) {
    uint8_t frame[512];
    uint32_t timestamp = micros();
    
    frame[0] = FRAME_HEADER_USB;
    frame[1] = 0;
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
    
    Serial.write(frame, frame_len);
    Serial.flush();
}
