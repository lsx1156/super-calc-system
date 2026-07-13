#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#include "config.h"
#include "system_status.h"
#include "cluster_mgr.h"
#include "spi_master.h"
#include "usb_cdc_comm.h"
#include "oc_control.h"
#include "data_aggregator.h"
#include "foolproof.h"
#include "addr_assigner.h"
#include "edge_compute.h"
#include "i2c_sched_master.h"
#include "dma_spi_harvester.h"

uint8_t current_mode = MODE_SAMPLE;
bool is_running = false;
uint32_t sample_rate = DEFAULT_SAMPLE_RATE;

void setup() {
    Serial.begin(115200);
    
    system_status_init();
    spi_master_init();
    cluster_init();
    usb_cdc_init();
    oc_control_init();
    data_aggregator_init();
    foolproof_init();
    addr_assigner_init();
    
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    
    adc_init();
    adc_set_temp_sensor_enabled(true);
    
    delay(500);
    
    cluster_detect_nodes();
    addr_assigner_scan_all();
    
    delay(100);
}

void process_usb_cmd(usb_cmd_t* cmd) {
    uint8_t resp[256];
    uint16_t resp_len = 0;
    system_status_t* st = system_status_get();
    cluster_state_t* cluster = cluster_get_state();
    
    switch (cmd->cmd) {
        case CMD_GET_STATUS: {
            resp[resp_len++] = DATA_STATUS;
            memcpy(&resp[resp_len], st, sizeof(system_status_t));
            resp_len += sizeof(system_status_t);
            
            resp[resp_len++] = cluster->total_nodes;
            resp[resp_len++] = cluster->online_count;
            resp[resp_len++] = cluster->fault_count;
            break;
        }
            
        case CMD_START_SAMPLE: {
            spi_master_broadcast(CMD_START_SAMPLE, NULL, 0);
            system_status_set_running(true);
            is_running = true;
            resp[resp_len++] = 0x01;
            break;
        }
            
        case CMD_STOP_SAMPLE: {
            spi_master_broadcast(CMD_STOP_SAMPLE, NULL, 0);
            system_status_set_running(false);
            is_running = false;
            resp[resp_len++] = 0x01;
            break;
        }
            
        case CMD_SET_RATE: {
            uint32_t rate = cmd->params[0] | (cmd->params[1] << 8)
                          | (cmd->params[2] << 16) | (cmd->params[3] << 24);
            st->sample_rate = rate;
            sample_rate = rate;
            spi_master_broadcast(CMD_SET_RATE, cmd->params, 4);
            resp[resp_len++] = 0x01;
            break;
        }
            
        case CMD_SET_MODE: {
            current_mode = cmd->params[0];
            system_status_set_mode(current_mode);
            spi_master_broadcast(CMD_SET_MODE, cmd->params, 1);
            
            if (current_mode == MODE_BRUTEFORCE) {
                oc_control_set(true);
                oc_control_set_all_pico(true);
            } else {
                oc_control_set(false);
                oc_control_set_all_pico(false);
            }
            resp[resp_len++] = 0x01;
            break;
        }
            
        case CMD_OVERCLOCK: {
            bool enable = cmd->params[0] == 1;
            oc_control_set(enable);
            oc_control_set_all_pico(enable);
            resp[resp_len++] = 0x01;
            break;
        }
            
        case CMD_NODE_DETECT: {
            cluster_detect_nodes();
            resp[resp_len++] = cluster->online_count;
            resp[resp_len++] = cluster->total_nodes;
            for (int i = 0; i < cluster->total_nodes; i++) {
                resp[resp_len++] = cluster->nodes[i].online ? 1 : 0;
            }
            break;
        }
            
        case CMD_SET_NODE_COUNT: {
            uint8_t count = cmd->params[0];
            if (count > 0 && count <= MAX_PICO_SLAVES) {
                cluster->total_nodes = count;
                st->pico_count = count;
                cluster_detect_nodes();
            }
            resp[resp_len++] = 0x01;
            break;
        }
            
        case CMD_RESET_NODE: {
            uint8_t node_id = cmd->params[0];
            cluster_reset_node(node_id);
            cluster_detect_nodes();
            resp[resp_len++] = 0x01;
            break;
        }
            
        case CMD_HW_TEST: {
            uint8_t node_id = cmd->params[0];
            uint8_t test_type = cmd->params[1];
            uint8_t node_resp[128];
            uint16_t node_resp_len = 0;
            if (spi_master_send_cmd(node_id, CMD_HW_TEST, &cmd->params[1], 1, 
                                    node_resp, &node_resp_len)) {
                memcpy(&resp[resp_len], node_resp, node_resp_len);
                resp_len += node_resp_len;
            }
            break;
        }
            
        case CMD_GLITCH: {
            uint16_t width = cmd->params[0] | (cmd->params[1] << 8);
            uint8_t node_id = cmd->params[2];
            uint8_t count = cmd->params[3];
            uint8_t glitch_params[3];
            glitch_params[0] = cmd->params[0];
            glitch_params[1] = cmd->params[1];
            glitch_params[2] = count;
            spi_master_send_cmd(node_id, CMD_GLITCH, glitch_params, 3, NULL, NULL);
            resp[resp_len++] = 0x01;
            break;
        }
            
        case CMD_GET_VERSION:
            resp[resp_len++] = FW_VERSION_MAJOR;
            resp[resp_len++] = FW_VERSION_MINOR;
            resp[resp_len++] = FW_VERSION_PATCH;
            break;
            
        default:
            resp[resp_len++] = 0xFF;
            break;
    }
    
    usb_cdc_send_data(resp, resp_len);
}

void aggregate_data(void) {
    uint8_t resp[256];
    uint16_t resp_len = 0;
    uint8_t temp_buf[512];
    uint32_t offset = 0;
    
    temp_buf[offset++] = DATA_AGGREGATED;
    temp_buf[offset++] = MAX_PICO_SLAVES;
    
    cluster_state_t* cluster = cluster_get_state();
    for (int i = 0; i < cluster->total_nodes; i++) {
        if (!cluster->nodes[i].online) continue;
        
        if (spi_master_send_cmd(i, CMD_GET_DATA, NULL, 0, resp, &resp_len)) {
            temp_buf[offset++] = i;
            temp_buf[offset++] = resp_len & 0xFF;
            temp_buf[offset++] = (resp_len >> 8) & 0xFF;
            memcpy(&temp_buf[offset], resp, resp_len);
            offset += resp_len;
        }
    }
    
    if (offset > 2) {
        usb_cdc_send_data(temp_buf, offset);
    }
    
    system_status_get()->total_samples++;
}

void loop() {
    usb_cdc_task();
    
    usb_cmd_t cmd;
    if (usb_cdc_get_cmd(&cmd)) {
        process_usb_cmd(&cmd);
    }
    
    if (is_running) {
        aggregate_data();
    }
    
    foolproof_task();
    
    static uint32_t last_temp_check = 0;
    uint32_t now = millis();
    if (now - last_temp_check >= TEMP_PERIOD_MS) {
        last_temp_check = now;
        
        adc_select_input(4);
        uint16_t val = adc_read();
        float voltage = val * 3.3f / 4095.0f;
        float temp = 27.0f - (voltage - 0.706f) / 0.001721f;
        
        system_status_update_temp(temp);
        oc_control_dynamic_adjust(temp);
    }
    
    system_status_get()->uptime_ms = millis();
    
    delay(1);
}