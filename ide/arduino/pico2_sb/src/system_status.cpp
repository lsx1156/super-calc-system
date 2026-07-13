#include "system_status.h"

static system_status_t g_status;

void system_status_init(void) {
    memset(&g_status, 0, sizeof(system_status_t));
    g_status.version_major = FW_VERSION_MAJOR;
    g_status.version_minor = FW_VERSION_MINOR;
    g_status.version_patch = FW_VERSION_PATCH;
    g_status.sample_rate = DEFAULT_SAMPLE_RATE;
    g_status.overclock_freq = DEFAULT_FREQ_KHZ;
    g_status.pico_count = DEFAULT_PICO_COUNT;
}

system_status_t* system_status_get(void) {
    return &g_status;
}

void system_status_set_running(bool running) {
    g_status.run_status = running ? 1 : 0;
}

void system_status_set_mode(uint8_t mode) {
    g_status.work_mode = mode;
}

void system_status_update_temp(float temp) {
    g_status.temperature = temp;
}

void system_status_add_error(void) {
    g_status.error_count++;
}