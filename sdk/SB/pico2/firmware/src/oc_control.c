#include "oc_control.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "pico/stdlib.h"
#include "spi_master.h"
#include "system_status.h"

static bool g_oc_active = false;

void oc_control_init(void) {
    g_oc_active = false;
}

bool oc_control_set(bool enable) {
    if (enable == g_oc_active) return true;
    
    if (enable) {
        vreg_set_voltage(VCORE_OVERCLOCK);
        sleep_ms(10);
        if (!set_sys_clock_khz(OVERCLOCK_FREQ_KHZ, true)) {
            vreg_set_voltage(VCORE_DEFAULT);
            return false;
        }
        g_oc_active = true;
    } else {
        set_sys_clock_khz(DEFAULT_FREQ_KHZ, true);
        vreg_set_voltage(VCORE_DEFAULT);
        g_oc_active = false;
    }
    
    system_status_t* st = system_status_get();
    st->overclock_freq = oc_control_get_freq();
    st->vcore = oc_control_get_vcore();
    
    return true;
}

bool oc_control_is_active(void) {
    return g_oc_active;
}

uint32_t oc_control_get_freq(void) {
    return g_oc_active ? OVERCLOCK_FREQ_KHZ : DEFAULT_FREQ_KHZ;
}

float oc_control_get_vcore(void) {
    return g_oc_active ? 1.20f : 1.10f;
}

bool oc_control_check_temp(float temp) {
    if (temp >= TEMP_SHUTDOWN && g_oc_active) {
        oc_control_set(false);
        return false;
    }
    return true;
}

void oc_control_dynamic_adjust(float temp) {
    if (temp >= TEMP_WARNING && g_oc_active) {
        oc_control_set(false);
    } else if (temp < 50.0f && !g_oc_active) {
        // 温度低时可恢复超频（需要外部触发）
    }
}

bool oc_control_set_all_pico(bool enable) {
    uint8_t param = enable ? 1 : 0;
    return spi_master_broadcast(CMD_OVERCLOCK, &param, 1);
}
