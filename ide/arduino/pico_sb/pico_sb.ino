#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#include "config.h"
#include "status_mgr.h"
#include "adc_sample.h"
#include "digital_capture.h"
#include "spi_comm.h"
#include "overclock.h"
#include "crack_engine.h"
#include "hw_test.h"
#include "i2c_sched_slave.h"
#include "i2c_sched_regs.h"

uint8_t current_mode = MODE_SAMPLE;
bool is_running = false;
uint32_t sample_rate = DEFAULT_SAMPLE_RATE;
uint8_t node_id = NODE_ID;

void setup() {
    Serial.begin(115200);
    
    status_init();
    adc_sample_init();
    digital_capture_init();
    spi_comm_init();
    overclock_init();
    crack_init();
    hw_test_init();
    
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    
    i2c_slave_config_t i2c_config = {
        .i2c_port = &Wire,
        .sda_pin = I2C_SCHED_SDA_PIN,
        .scl_pin = I2C_SCHED_SCL_PIN,
        .node_id = NODE_ID,
        .cluster_id = 0,
        .bus_freq = I2C_SCHED_FREQ,
        .start_unassigned = true
    };
    i2c_sched_slave_init(&i2c_config);
    
    delay(100);
}

void process_cmd(spi_cmd_t* cmd) {
    uint8_t resp[128];
    uint16_t resp_len = 0;
    device_status_t* st = status_get();
    
    switch (cmd->cmd) {
        case CMD_GET_STATUS:
            resp[resp_len++] = DATA_STATUS;
            memcpy(&resp[resp_len], st, sizeof(device_status_t));
            resp_len += sizeof(device_status_t);
            break;
            
        case CMD_GET_VERSION:
            resp[resp_len++] = FW_VERSION_MAJOR;
            resp[resp_len++] = FW_VERSION_MINOR;
            resp[resp_len++] = FW_VERSION_PATCH;
            break;
            
        case CMD_START_SAMPLE:
            adc_sample_start(st->sample_rate);
            digital_capture_start(st->sample_rate);
            status_set_running(true);
            status_reset_counts();
            is_running = true;
            resp[resp_len++] = 0x01;
            break;
            
        case CMD_STOP_SAMPLE:
            adc_sample_stop();
            digital_capture_stop();
            status_set_running(false);
            is_running = false;
            resp[resp_len++] = 0x01;
            break;
            
        case CMD_SET_RATE: {
            uint32_t rate = cmd->params[0] | (cmd->params[1] << 8)
                          | (cmd->params[2] << 16) | (cmd->params[3] << 24);
            st->sample_rate = rate;
            sample_rate = rate;
            if (is_running) {
                adc_sample_stop();
                digital_capture_stop();
                adc_sample_start(rate);
                digital_capture_start(rate);
            }
            resp[resp_len++] = 0x01;
            break;
        }
            
        case CMD_SET_MODE: {
            uint8_t old_mode = current_mode;
            current_mode = cmd->params[0];
            status_set_mode(current_mode);
            
            if (current_mode == MODE_BRUTEFORCE) {
                overclock_set_ex(true, 2);
                crack_start("00000000000000000000000000000000", 8, "0123456789abcdef");
            } else {
                overclock_set_ex(false, 2);
                if (old_mode == MODE_BRUTEFORCE) {
                    crack_stop();
                }
            }
            resp[resp_len++] = 0x01;
            break;
        }
            
        case CMD_OVERCLOCK:
            overclock_set_ex(cmd->params[0] == 1, 1);
            st->overclock_freq = overclock_get_freq();
            resp[resp_len++] = 0x01;
            break;
            
        case CMD_GET_DATA: {
            resp[resp_len++] = DATA_ANALOG;
            uint16_t vals[4];
            if (adc_sample_read(vals, 4)) {
                for (int i = 0; i < 4; i++) {
                    resp[resp_len++] = vals[i] & 0xFF;
                    resp[resp_len++] = (vals[i] >> 8) & 0xFF;
                }
            }
            break;
        }
            
        case CMD_HW_TEST:
            hw_test_run(cmd->params[0], resp, &resp_len);
            break;
            
        case CMD_GLITCH: {
            uint16_t width = cmd->params[0] | (cmd->params[1] << 8);
            uint8_t count = cmd->params[2];
            hw_test_glitch(width, count);
            resp[resp_len++] = 0x01;
            break;
        }
        
        case CMD_ADDR_ASSIGN: {
            uint8_t slot_id = cmd->params[0];
            uint8_t i2c_addr = I2C_ADDR_BASE + slot_id;
            
            if (i2c_sched_slave_is_assigned()) {
                resp[resp_len++] = 0x10;
                resp[resp_len++] = i2c_sched_slave_get_address();
            } else if (i2c_sched_slave_set_address(i2c_addr)) {
                node_id = slot_id;
                st->node_id = slot_id;
                resp[resp_len++] = 0x06;
                resp[resp_len++] = i2c_addr;
            } else {
                resp[resp_len++] = 0x15;
            }
            break;
        }
        
        case CMD_ADDR_QUERY: {
            if (i2c_sched_slave_is_assigned()) {
                resp[resp_len++] = 0x06;
                resp[resp_len++] = i2c_sched_slave_get_address();
            } else {
                resp[resp_len++] = 0x15;
                resp[resp_len++] = I2C_ADDR_UNASSIGNED;
            }
            break;
        }
        
        case CMD_ADDR_CLEAR: {
            if (i2c_sched_slave_is_assigned()) {
                i2c_sched_slave_clear_address();
                resp[resp_len++] = 0x06;
            } else {
                resp[resp_len++] = 0x10;
            }
            break;
        }
            
        case CMD_RESET:
            NVIC_SystemReset();
            break;
            
        case CMD_NOP:
        default:
            resp[resp_len++] = 0x00;
            break;
    }
    
    spi_comm_send_resp(resp, resp_len);
}

void loop() {
    spi_comm_task();
    
    spi_cmd_t cmd;
    if (spi_comm_get_cmd(&cmd)) {
        process_cmd(&cmd);
    }
    
    if (is_running) {
        device_status_t* st = status_get();
        st->total_samples++;
        
        if (st->total_samples % 1000 == 0) {
            float temp = adc_get_temp();
            status_update_temp(temp);
            overclock_check_temp(temp);
        }
    }
    
    crack_loop();
    
    sync_event_t event = i2c_sched_slave_get_event();
    if (event != SYNC_EVENT_NONE) {
        switch (event) {
            case SYNC_EVENT_START: {
                uint8_t mode = i2c_sched_slave_get_reg(REG_MODE);
                uint8_t rate_code = i2c_sched_slave_get_reg(REG_SAMPLE_RATE);
                uint32_t rate = RATE_TO_HZ(rate_code);
                
                current_mode = mode;
                status_set_mode(mode);
                sample_rate = rate;
                device_status_t* st = status_get();
                st->sample_rate = rate;
                
                adc_sample_start(rate);
                digital_capture_start(rate);
                status_set_running(true);
                status_reset_counts();
                is_running = true;
                break;
            }
            case SYNC_EVENT_STOP:
                adc_sample_stop();
                digital_capture_stop();
                status_set_running(false);
                is_running = false;
                break;
            case SYNC_EVENT_RESET:
                status_reset_counts();
                adc_sample_stop();
                digital_capture_stop();
                status_set_running(false);
                is_running = false;
                break;
            default:
                break;
        }
        i2c_sched_slave_clear_event();
    }
    
    if (i2c_sched_slave_regs_changed()) {
        device_status_t* st = status_get();
        uint8_t rate_code = i2c_sched_slave_get_reg(REG_SAMPLE_RATE);
        uint32_t rate = RATE_TO_HZ(rate_code);
        st->sample_rate = rate;
        
        uint8_t oc_mode = i2c_sched_slave_get_reg(REG_OVERCLOCK);
        if (oc_mode == OC_200MHZ) {
            overclock_set_ex(true, 3);
        } else if (oc_mode == OC_DISABLED) {
            overclock_set_ex(false, 3);
        }
    }
    
    device_status_t* st = status_get();
    if (st->run_status && st->total_samples % 100 == 0) {
        i2c_sched_slave_update_status(STATUS_RUNNING, true);
        i2c_sched_slave_update_status(STATUS_SYNC_LOCKED, true);
        i2c_sched_slave_update_status(STATUS_WATCHDOG_OK, true);
    }
    
    delay(1);
}