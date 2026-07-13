#include "status_mgr.h"

static device_status_t g_status;
static bool g_mutex_locked = false;

void status_init(void) {
    memset(&g_status, 0, sizeof(device_status_t));
    g_status.version_major = FW_VERSION_MAJOR;
    g_status.version_minor = FW_VERSION_MINOR;
    g_status.version_patch = FW_VERSION_PATCH;
    g_status.sample_rate = DEFAULT_SAMPLE_RATE;
    g_status.node_id = NODE_ID;
    g_status.overclock_freq = DEFAULT_FREQ_KHZ;
}

void status_update_temp(float temp) {
    if (status_acquire_mutex()) {
        g_status.core_temp = temp;
        status_release_mutex();
    }
}

void status_set_mode(uint8_t mode) {
    if (status_acquire_mutex()) {
        g_status.work_mode = mode;
        status_release_mutex();
    }
}

void status_set_running(bool running) {
    if (status_acquire_mutex()) {
        g_status.run_status = running ? 1 : 0;
        status_release_mutex();
    }
}

void status_add_error(void) {
    if (status_acquire_mutex()) {
        g_status.error_count++;
        status_release_mutex();
    }
}

void status_reset_counts(void) {
    if (status_acquire_mutex()) {
        g_status.total_samples = 0;
        g_status.error_count = 0;
        status_release_mutex();
    }
}

device_status_t* status_get(void) {
    return &g_status;
}

bool status_acquire_mutex(void) {
    while (g_mutex_locked) {
        delayMicroseconds(1);
    }
    g_mutex_locked = true;
    return true;
}

void status_release_mutex(void) {
    g_mutex_locked = false;
}