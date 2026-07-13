#include "oc_control.h"
#include "config.h"
#include "spi_master.h"

static bool g_overclock_enabled = false;

void oc_control_init(void) {
    g_overclock_enabled = false;
}

void oc_control_set(bool enable) {
    if (enable) {
        set_cpu_freq(240);
        g_overclock_enabled = true;
    } else {
        set_cpu_freq(150);
        g_overclock_enabled = false;
    }
}

void oc_control_set_all_pico(bool enable) {
    uint8_t param = enable ? 1 : 0;
    spi_master_broadcast(CMD_OVERCLOCK, &param, 1);
}

void oc_control_dynamic_adjust(float temp) {
    if (g_overclock_enabled && temp >= TEMP_SHUTDOWN) {
        oc_control_set(false);
        oc_control_set_all_pico(false);
    }
}

uint32_t oc_control_get_freq(void) {
    return g_overclock_enabled ? OVERCLOCK_FREQ_KHZ : DEFAULT_FREQ_KHZ;
}