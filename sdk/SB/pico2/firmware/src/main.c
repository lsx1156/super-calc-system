#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/watchdog.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include "FreeRTOS.h"
#include "task.h"

#include "config.h"
#include "system_status.h"
#include "cluster_mgr.h"
#include "spi_master.h"
#include "dma_spi_harvester.h"
#include "edge_compute.h"
#include "data_aggregator.h"
#include "usb_cdc_comm.h"
#include "foolproof.h"
#include "oc_control.h"
#include "i2c_sched_master.h"
#include "addr_assigner.h"

static TaskHandle_t g_foolproof_task = NULL;
static TaskHandle_t g_spi_task = NULL;
static TaskHandle_t g_dma_harvest_task = NULL;
static TaskHandle_t g_aggregate_task = NULL;
static TaskHandle_t g_usb_task = NULL;
static TaskHandle_t g_temp_task = NULL;
static TaskHandle_t g_i2c_sched_task = NULL;

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

// ==================== 初始化任务 ====================
static void init_task(void* pvParameters) {
    vTaskDelay(pdMS_TO_TICKS(100));
    
    cluster_detect_nodes();
    
    addr_assigner_init();
    addr_assigner_scan_all();
    
    i2c_sched_detect_nodes();
    
    vTaskDelete(NULL);
}

// ==================== I2C调度任务 ====================
static void i2c_sched_task(void* pvParameters) {
    i2c_master_config_t config = {
        .i2c_port = I2C_SCHED_PORT,
        .sda_pin = I2C_SCHED_SDA_PIN,
        .scl_pin = I2C_SCHED_SCL_PIN,
        .bus_freq = I2C_SCHED_FREQ,
        .max_nodes = MAX_PICO_SLAVES
    };
    
    i2c_status_t ret = i2c_sched_master_init(&config);
    if (ret != I2C_OK) {
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    
    i2c_sched_detect_nodes();
    
    while (1) {
        i2c_sched_sync_update();
        
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ==================== 温度监控任务 ====================
static void temp_monitor_task(void* pvParameters) {
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);
    
    while (1) {
        uint16_t val = adc_read();
        float voltage = val * 3.3f / 4095.0f;
        float temp = 27.0f - (voltage - 0.706f) / 0.001721f;
        
        system_status_update_temp(temp);
        oc_control_dynamic_adjust(temp);
        
        vTaskDelay(pdMS_TO_TICKS(TEMP_PERIOD_MS));
    }
}

// ==================== 命令处理 ====================
static void process_usb_cmd(usb_cmd_t* cmd) {
    uint8_t resp[512];
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
            uint8_t param = 1;
            spi_master_broadcast(CMD_START_SAMPLE, NULL, 0);
            dma_spi_harvester_start();
            system_status_set_running(true);
            resp[resp_len++] = 0x01;
            break;
        }
            
        case CMD_STOP_SAMPLE: {
            spi_master_broadcast(CMD_STOP_SAMPLE, NULL, 0);
            dma_spi_harvester_stop();
            system_status_set_running(false);
            resp[resp_len++] = 0x01;
            break;
        }
            
        case CMD_SET_RATE: {
            uint32_t rate = cmd->params[0] | (cmd->params[1] << 8)
                          | (cmd->params[2] << 16) | (cmd->params[3] << 24);
            st->sample_rate = rate;
            spi_master_broadcast(CMD_SET_RATE, cmd->params, 4);
            resp[resp_len++] = 0x01;
            break;
        }
            
        case CMD_SET_MODE: {
            system_status_set_mode(cmd->params[0]);
            spi_master_broadcast(CMD_SET_MODE, cmd->params, 1);
            
            if (cmd->params[0] == MODE_BRUTEFORCE) {
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

// ==================== 命令分发任务 ====================
static void cmd_dispatch_task(void* pvParameters) {
    usb_cmd_t cmd;
    uint8_t pkt_buf[1024];
    uint16_t pkt_len = 0;
    
    while (1) {
        if (usb_cdc_get_cmd(&cmd)) {
            process_usb_cmd(&cmd);
        }
        
        if (system_status_get()->run_status) {
            if (data_aggregator_get_packet(pkt_buf, &pkt_len)) {
                usb_cdc_send_data(pkt_buf, pkt_len);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ==================== 主函数 ====================
int main(void) {
    // 系统时钟初始化
    set_sys_clock_khz(DEFAULT_FREQ_KHZ, true);
    
    // 看门狗
    watchdog_enable(WATCHDOG_TIMEOUT, 1);
    
    // 模块初始化
    system_status_init();
    spi_master_init();
    cluster_init();
    data_aggregator_init();
    usb_cdc_init();
    foolproof_init();
    oc_control_init();
    
    adc_init();
    
    // LED
    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);
    
    // 创建FreeRTOS任务（按优先级从高到低）
    xTaskCreate(foolproof_task, "FOOLPROOF", TASK_STACK_FOOLPROOF, NULL, 
                TASK_PRI_FOOLPROOF, &g_foolproof_task);
    
    xTaskCreate(data_aggregator_task, "AGG", TASK_STACK_AGGREGATE, NULL, 
                TASK_PRI_AGGREGATE, &g_aggregate_task);
    
    xTaskCreate(dma_spi_harvester_task, "DMA_HARV", 256, NULL, 
                TASK_PRI_AGGREGATE, &g_dma_harvest_task);
    
    xTaskCreate(usb_cdc_task, "USB", TASK_STACK_USB, NULL, 
                TASK_PRI_USB, &g_usb_task);
    
    xTaskCreate(i2c_sched_task, "I2C_SCHED", 256, NULL, 
                TASK_PRI_AGGREGATE, &g_i2c_sched_task);
    
    xTaskCreate(temp_monitor_task, "TEMP", TASK_STACK_TEMP, NULL, 
                TASK_PRI_TEMP, &g_temp_task);
    
    xTaskCreate(cmd_dispatch_task, "CMD", 512, NULL, TASK_PRI_USB, NULL);
    
    xTaskCreate(init_task, "INIT", 256, NULL, 1, NULL);
    
    // 启动调度器
    vTaskStartScheduler();
    
    while (1) {
        tight_loop_contents();
    }
    
    return 0;
}
