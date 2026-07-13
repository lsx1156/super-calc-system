#include "system_status.h"
#include <string.h>

static system_status_t g_status;

void system_status_init(void) {
    memset(&g_status, 0, sizeof(g_status));
    g_status.work_mode = MODE_SAMPLE;
    g_status.run_status = 0;
    g_status.sample_rate = 50000;
    g_status.overclock_freq = DEFAULT_FREQ_KHZ;
    g_status.core_temp = 25.0f;
    g_status.vcore = 1.1f;
    g_status.fault_code = 0;
    g_status.version_major = FW_VERSION_MAJOR;
    g_status.version_minor = FW_VERSION_MINOR;
    g_status.version_patch = FW_VERSION_PATCH;
    g_status.pico_count = DEFAULT_PICO_COUNT;
    g_status.online_count = 0;
}

void system_status_set_mode(uint8_t mode) {
    g_status.work_mode = mode;
}

void system_status_set_running(bool running) {
    g_status.run_status = running ? 1 : 0;
}

void system_status_update_temp(float temp) {
    g_status.core_temp = temp;
}

system_status_t* system_status_get(void) {
    return &g_status;
}
