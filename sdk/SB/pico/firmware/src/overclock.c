#include "overclock.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "semphr.h"

static bool g_oc_active = false;
static uint8_t g_oc_source = OC_SOURCE_NONE;
static SemaphoreHandle_t g_oc_mutex = NULL;

void overclock_init(void) {
    g_oc_active = false;
    g_oc_source = OC_SOURCE_NONE;
    g_oc_mutex = xSemaphoreCreateMutex();
}

bool overclock_acquire(void) {
    if (g_oc_mutex == NULL) return false;
    return xSemaphoreTake(g_oc_mutex, pdMS_TO_TICKS(100)) == pdTRUE;
}

void overclock_release(void) {
    if (g_oc_mutex != NULL) {
        xSemaphoreGive(g_oc_mutex);
    }
}

bool overclock_set_ex(bool enable, uint8_t source) {
    if (!overclock_acquire()) return false;
    
    if (enable == g_oc_active) {
        g_oc_source = source;
        overclock_release();
        return true;
    }
    
    if (enable) {
        vreg_set_voltage(VCORE_OVERCLOCK);
        sleep_ms(10);
        if (!set_sys_clock_khz(OVERCLOCK_FREQ_KHZ, true)) {
            vreg_set_voltage(VCORE_DEFAULT);
            overclock_release();
            return false;
        }
        g_oc_active = true;
        g_oc_source = source;
    } else {
        set_sys_clock_khz(DEFAULT_FREQ_KHZ, true);
        vreg_set_voltage(VCORE_DEFAULT);
        g_oc_active = false;
        g_oc_source = OC_SOURCE_NONE;
    }
    
    overclock_release();
    return true;
}

bool overclock_set(bool enable) {
    return overclock_set_ex(enable, OC_SOURCE_UNKNOWN);
}

bool overclock_is_active(void) {
    return g_oc_active;
}

uint32_t overclock_get_freq(void) {
    return g_oc_active ? OVERCLOCK_FREQ_KHZ : DEFAULT_FREQ_KHZ;
}

float overclock_get_vcore(void) {
    return g_oc_active ? 1.20f : 1.10f;
}

uint8_t overclock_get_source(void) {
    return g_oc_source;
}

bool overclock_check_temp(float temp) {
    if (temp >= TEMP_SHUTDOWN && g_oc_active) {
        overclock_set_ex(false, OC_SOURCE_TEMP_PROTECT);
        return false;
    }
    return true;
}
