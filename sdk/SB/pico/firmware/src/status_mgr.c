#include "status_mgr.h"
#include <string.h>
#include "FreeRTOS.h"
#include "semphr.h"

static device_status_t g_status;
static SemaphoreHandle_t g_status_mutex = NULL;

void status_init(void) {
    memset(&g_status, 0, sizeof(g_status));
    g_status.work_mode = MODE_SAMPLE;
    g_status.run_status = 0;
    g_status.sample_rate = DEFAULT_SAMPLE_RATE;
    g_status.overclock_freq = DEFAULT_FREQ_KHZ;
    g_status.core_temp = 25.0f;
    g_status.vcore = 1.1f;
    g_status.version_major = FW_VERSION_MAJOR;
    g_status.version_minor = FW_VERSION_MINOR;
    g_status.version_patch = FW_VERSION_PATCH;
    
    g_status_mutex = xSemaphoreCreateMutex();
}

bool status_acquire_mutex(void) {
    if (g_status_mutex == NULL) return false;
    return xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(100)) == pdTRUE;
}

void status_release_mutex(void) {
    if (g_status_mutex != NULL) {
        xSemaphoreGive(g_status_mutex);
    }
}

void status_update_temp(float temp) {
    g_status.core_temp = temp;
}

void status_set_mode(uint8_t mode) {
    g_status.work_mode = mode;
}

void status_set_running(bool running) {
    g_status.run_status = running ? 1 : 0;
}

void status_add_error(void) {
    g_status.error_count++;
}

void status_reset_counts(void) {
    g_status.total_samples = 0;
    g_status.error_count = 0;
}

void status_reset(void) {
    memset(&g_status, 0, sizeof(g_status));
    g_status.work_mode = MODE_SAMPLE;
    g_status.run_status = 0;
    g_status.sample_rate = DEFAULT_SAMPLE_RATE;
    g_status.overclock_freq = DEFAULT_FREQ_KHZ;
    g_status.core_temp = 25.0f;
    g_status.vcore = 1.1f;
    g_status.version_major = FW_VERSION_MAJOR;
    g_status.version_minor = FW_VERSION_MINOR;
    g_status.version_patch = FW_VERSION_PATCH;
}

device_status_t* status_get(void) {
    return &g_status;
}
