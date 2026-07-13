#include "overclock.h"
#include "config.h"

static bool g_overclock_enabled = false;
static int g_overclock_source = OC_SOURCE_NONE;

void overclock_init(void) {
    g_overclock_enabled = false;
    g_overclock_source = OC_SOURCE_NONE;
}

void overclock_set(bool enable) {
    overclock_set_ex(enable, OC_SOURCE_SPI_CMD);
}

void overclock_set_ex(bool enable, int source) {
    if (enable) {
        set_cpu_freq(200);
        g_overclock_enabled = true;
        g_overclock_source = source;
    } else {
        set_cpu_freq(133);
        g_overclock_enabled = false;
        g_overclock_source = OC_SOURCE_NONE;
    }
}

uint32_t overclock_get_freq(void) {
    return g_overclock_enabled ? OVERCLOCK_FREQ_KHZ : DEFAULT_FREQ_KHZ;
}

void overclock_check_temp(float temp) {
    if (g_overclock_enabled && temp >= TEMP_SHUTDOWN) {
        overclock_set_ex(false, OC_SOURCE_NONE);
    }
}