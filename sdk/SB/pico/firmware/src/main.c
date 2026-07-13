#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/watchdog.h"
#include "hardware/clocks.h"
#include "FreeRTOS.h"
#include "task.h"

#include "config.h"
#include "status_mgr.h"
#include "adc_sample.h"
#include "digital_capture.h"
#include "spi_comm.h"
#include "overclock.h"
#include "crack_engine.h"
#include "hw_test.h"
#include "i2c_sched_slave.h"
#include "addr_assign_protocol.h"

static TaskHandle_t g_spi_task = NULL;
static TaskHandle_t g_sample_task = NULL;
static TaskHandle_t g_crack_task = NULL;
static TaskHandle_t g_monitor_task = NULL;
static TaskHandle_t g_i2c_slave_task = NULL;

// ==================== FreeRTOS Hook 函数 ====================
void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    taskDISABLE_INTERRUPTS();
    while (1) {
        tight_loop_contents();
    }
}

void vApplicationMallocFailedHook(void) {
    taskDISABLE_INTERRUPTS();
    while (1) {
        tight_loop_contents();
    }
}

// ==================== I2C从机回调 ====================
static void i2c_sync_callback(sync_event_t event, uint8_t reg_addr, uint8_t value) {
    (void)reg_addr;
    (void)value;
}

// ==================== I2C从机任务 ====================
static void i2c_slave_task(void* pvParameters) {
    i2c_slave_config_t config = {
        .i2c_port = I2C_SCHED_PORT,
        .sda_pin = I2C_SCHED_SDA_PIN,
        .scl_pin = I2C_SCHED_SCL_PIN,
        .node_id = NODE_ID,
        .cluster_id = 0,
        .bus_freq = I2C_SCHED_FREQ,
        .start_unassigned = true
    };
    
    i2c_status_t ret = i2c_sched_slave_init(&config);
    if (ret != I2C_OK) {
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    
    i2c_sched_slave_set_callback(i2c_sync_callback);
    
    while (1) {
        sync_event_t event = i2c_sched_slave_get_event();
        if (event != SYNC_EVENT_NONE) {
            if (status_acquire_mutex()) {
                switch (event) {
                    case SYNC_EVENT_START: {
                        uint8_t mode = i2c_sched_slave_get_reg(REG_MODE);
                        uint8_t rate_code = i2c_sched_slave_get_reg(REG_SAMPLE_RATE);
                        
                        status_set_mode(mode);
                        uint32_t rate = RATE_TO_HZ(rate_code);
                        adc_sample_start(rate);
                        digital_capture_start(rate);
                        status_set_running(true);
                        status_reset_counts();
                        break;
                    }
                    case SYNC_EVENT_STOP:
                        adc_sample_stop();
                        digital_capture_stop();
                        status_set_running(false);
                        break;
                    case SYNC_EVENT_RESET:
                        status_reset();
                        adc_sample_stop();
                        digital_capture_stop();
                        status_set_running(false);
                        break;
                    default:
                        break;
                }
                status_release_mutex();
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
                overclock_set_ex(true, OC_SOURCE_I2C_REG);
            } else if (oc_mode == OC_DISABLED) {
                overclock_set_ex(false, OC_SOURCE_I2C_REG);
            }
        }
        
        device_status_t* st = status_get();
        if (st->run_status && st->total_samples % 100 == 0) {
            i2c_sched_slave_update_status(STATUS_RUNNING, true);
            i2c_sched_slave_update_status(STATUS_SYNC_LOCKED, true);
            i2c_sched_slave_update_status(STATUS_WATCHDOG_OK, true);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ==================== 采样任务 ====================
static void sample_task(void* pvParameters) {
    while (1) {
        device_status_t* st = status_get();
        
        if (st->run_status) {
            if (st->work_mode == MODE_SAMPLE || st->work_mode == MODE_CRACK) {
                st->total_samples++;
                
                if (st->total_samples % 1000 == 0) {
                    float temp = adc_get_temp();
                    status_update_temp(temp);
                    overclock_check_temp(temp);
                }
            }
        }
        
        watchdog_update();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ==================== 监控任务 ====================
static void monitor_task(void* pvParameters) {
    while (1) {
        device_status_t* st = status_get();
        
        // 温度监控
        if (st->core_temp >= TEMP_SHUTDOWN) {
            if (status_acquire_mutex()) {
                status_set_running(false);
                adc_sample_stop();
                digital_capture_stop();
                overclock_set(false);
                status_release_mutex();
            }
        }
        
        st->uptime_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ==================== 命令处理 ====================
static void process_cmd(spi_cmd_t* cmd) {
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
            if (status_acquire_mutex()) {
                adc_sample_start(st->sample_rate);
                digital_capture_start(st->sample_rate);
                status_set_running(true);
                status_reset_counts();
                status_release_mutex();
            }
            resp[resp_len++] = 0x01;
            break;
            
        case CMD_STOP_SAMPLE:
            if (status_acquire_mutex()) {
                adc_sample_stop();
                digital_capture_stop();
                status_set_running(false);
                status_release_mutex();
            }
            resp[resp_len++] = 0x01;
            break;
            
        case CMD_SET_RATE: {
            uint32_t rate = cmd->params[0] | (cmd->params[1] << 8)
                          | (cmd->params[2] << 16) | (cmd->params[3] << 24);
            st->sample_rate = rate;
            if (st->run_status) {
                adc_sample_stop();
                digital_capture_stop();
                adc_sample_start(rate);
                digital_capture_start(rate);
            }
            resp[resp_len++] = 0x01;
            break;
        }
            
        case CMD_SET_MODE: {
            uint8_t old_mode = st->work_mode;
            status_set_mode(cmd->params[0]);
            
            if (cmd->params[0] == MODE_BRUTEFORCE || cmd->params[0] == MODE_CRACK) {
                overclock_set_ex(true, OC_SOURCE_SPI_MODE);
                digital_capture_set_mode(true);
                crack_start("00000000000000000000000000000000", 8, "0123456789abcdef");
            } else {
                overclock_set_ex(false, OC_SOURCE_SPI_MODE);
                digital_capture_set_mode(false);
                if (old_mode == MODE_BRUTEFORCE || old_mode == MODE_CRACK) {
                    crack_stop();
                }
            }
            resp[resp_len++] = 0x01;
            break;
        }
            
        case CMD_OVERCLOCK:
            overclock_set_ex(cmd->params[0] == 1, OC_SOURCE_SPI_CMD);
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
            resp[resp_len++] = 0x00;
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
            uint8_t i2c_addr = SLOT_TO_I2C_ADDR(slot_id);
            
            if (i2c_sched_slave_is_assigned()) {
                resp[resp_len++] = 0x10;
                resp[resp_len++] = i2c_sched_slave_get_address();
            } else if (i2c_sched_slave_set_address(i2c_addr)) {
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
            watchdog_reboot(0, 0, 100);
            break;
            
        case CMD_NOP:
        default:
            resp[resp_len++] = 0x00;
            break;
    }
    
    spi_comm_send_resp(resp, resp_len);
}

// ==================== SPI命令分发任务 ====================
static void spi_cmd_dispatch_task(void* pvParameters) {
    spi_cmd_t cmd;
    
    while (1) {
        if (spi_comm_get_cmd(&cmd)) {
            process_cmd(&cmd);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ==================== 主函数 ====================
int main(void) {
    // 系统时钟初始化
    set_sys_clock_khz(DEFAULT_FREQ_KHZ, true);
    
    // 看门狗初始化
    watchdog_enable(WATCHDOG_TIMEOUT, 1);
    
    // 模块初始化
    status_init();
    adc_sample_init();
    digital_capture_init();
    spi_comm_init();
    overclock_init();
    crack_init();
    hw_test_init();
    
    // LED
    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);
    
    // 创建FreeRTOS任务
    xTaskCreate(spi_comm_task, "SPI", TASK_STACK_SPI, NULL, TASK_PRI_SPI, &g_spi_task);
    xTaskCreate(spi_cmd_dispatch_task, "SPI_CMD", TASK_STACK_SPI, NULL, TASK_PRI_SPI, NULL);
    xTaskCreate(i2c_slave_task, "I2C_SLAVE", TASK_STACK_SPI, NULL, TASK_PRI_SPI, &g_i2c_slave_task);
    xTaskCreate(sample_task, "SAMPLE", TASK_STACK_SAMPLE, NULL, TASK_PRI_SAMPLE, &g_sample_task);
    xTaskCreate(crack_task, "CRACK", TASK_STACK_CRACK, NULL, TASK_PRI_CRACK, &g_crack_task);
    xTaskCreate(monitor_task, "MON", TASK_STACK_MONITOR, NULL, TASK_PRI_MONITOR, &g_monitor_task);
    
    // 启动调度器
    vTaskStartScheduler();
    
    while (1) {
        tight_loop_contents();
    }
    
    return 0;
}
