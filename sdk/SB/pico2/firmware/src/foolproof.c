#include "foolproof.h"
#include "system_status.h"
#include "cluster_mgr.h"
#include "oc_control.h"
#include "spi_master.h"
#include <string.h>
#include "hardware/watchdog.h"
#include "hardware/gpio.h"

#define MAX_FAULTS 16
static uint8_t g_faults[MAX_FAULTS];
static uint8_t g_fault_count = 0;
static uint32_t g_last_check_ms = 0;
static bool g_fault_led_state = false;

void foolproof_init(void) {
    memset(g_faults, 0, sizeof(g_faults));
    g_fault_count = 0;
    g_fault_led_state = false;
    
    gpio_init(PRSNT_PIN);
    gpio_set_dir(PRSNT_PIN, GPIO_IN);
    gpio_pull_up(PRSNT_PIN);
    
    gpio_init(FAULT_LED_PIN);
    gpio_set_dir(FAULT_LED_PIN, GPIO_OUT);
    gpio_put(FAULT_LED_PIN, 0);
}

void foolproof_set_fault_led(bool on) {
    g_fault_led_state = on;
    gpio_put(FAULT_LED_PIN, on ? 1 : 0);
}

bool foolproof_check_temperature(void) {
    system_status_t* st = system_status_get();
    
    if (st->core_temp >= TEMP_SHUTDOWN) {
        foolproof_trigger_emergency();
        return false;
    }
    
    if (st->core_temp >= TEMP_WARNING) {
        oc_control_set(false);
        oc_control_set_all_pico(false);
    }
    
    return true;
}

bool foolproof_check_nodes(void) {
    cluster_state_t* cluster = cluster_get_state();
    
    if (cluster->fault_count > 0) {
        cluster_auto_heal();
    }
    
    if (cluster->online_count == 0 && system_status_get()->run_status) {
        return false;
    }
    
    return true;
}

bool foolproof_check_spi(void) {
    cluster_state_t* cluster = cluster_get_state();
    uint8_t error_nodes = 0;
    
    for (int i = 0; i < cluster->total_nodes; i++) {
        if (cluster->nodes[i].error_count >= MAX_ERROR_COUNT / 2) {
            error_nodes++;
        }
    }
    
    if (error_nodes > cluster->total_nodes / 2) {
        return false;
    }
    
    return true;
}

bool foolproof_check_interface(void) {
    if (gpio_get(PRSNT_PIN) == 1) {
        foolproof_set_fault_led(true);
        return false;
    }
    
    foolproof_set_fault_led(false);
    return true;
}

bool foolproof_check_all(void) {
    bool ok = true;
    
    if (!foolproof_check_temperature()) ok = false;
    if (!foolproof_check_nodes()) ok = false;
    if (!foolproof_check_spi()) ok = false;
    if (!foolproof_check_interface()) ok = false;
    
    return ok;
}

void foolproof_trigger_emergency(void) {
    system_status_t* st = system_status_get();
    
    st->run_status = 0;
    
    oc_control_set(false);
    oc_control_set_all_pico(false);
    
    uint8_t param = 0;
    spi_master_broadcast(CMD_STOP_SAMPLE, &param, 1);
    
    watchdog_enable(1000, 1);
}

void foolproof_clear_fault(uint8_t fault_code) {
    for (int i = 0; i < g_fault_count; i++) {
        if (g_faults[i] == fault_code) {
            for (int j = i; j < g_fault_count - 1; j++) {
                g_faults[j] = g_faults[j + 1];
            }
            g_fault_count--;
            break;
        }
    }
}

uint8_t foolproof_get_fault_count(void) {
    return g_fault_count;
}

bool foolproof_has_fault(void) {
    return g_fault_count > 0;
}

void foolproof_task(void* pvParameters) {
    while (1) {
        foolproof_check_all();
        watchdog_update();
        vTaskDelay(pdMS_TO_TICKS(FOOLPROOF_PERIOD_MS));
    }
}
