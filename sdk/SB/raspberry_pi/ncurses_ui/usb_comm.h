#ifndef USB_COMM_H
#define USB_COMM_H

#include <stdint.h>

#define USB_PORT "/dev/ttyACM0"
#define USB_BAUDRATE 100000000

#define FRAME_HEADER 0x55
#define FRAME_TAIL 0xAA

#define CMD_NOP 0x00
#define CMD_GET_STATUS 0x01
#define CMD_START_SAMPLE 0x02
#define CMD_STOP_SAMPLE 0x03
#define CMD_SET_RATE 0x04
#define CMD_GET_DATA 0x05
#define CMD_SET_MODE 0x06
#define CMD_GET_VERSION 0x07
#define CMD_OVERCLOCK 0x08
#define CMD_HW_TEST 0x09
#define CMD_NODE_DETECT 0x0B
#define CMD_BROADCAST 0x0E
#define CMD_RESET 0xFF

#define MODE_SAMPLE 0x00
#define MODE_CRACK 0x01
#define MODE_BRUTEFORCE 0x02
#define MODE_HW_TEST 0x03
#define MODE_STANDBY 0xFF

typedef struct {
    uint8_t work_mode;
    uint8_t run_status;
    uint8_t node_id;
    uint32_t sample_rate;
    uint32_t total_samples;
    uint32_t overclock_freq;
    float core_temp;
    float vcore;
    uint32_t error_count;
    uint8_t fault_code;
    uint32_t uptime_ms;
} device_status_t;

typedef struct {
    int fd;
    struct termios oldtio;
    uint8_t rx_buffer[4096];
    int rx_len;
} usb_handle_t;

typedef struct {
    int pico2_count;
    int pico2_online;
    int pico_count;
    int pico_online;
    device_status_t pico2_status;
    device_status_t pico_status[16];
    float pi_temp;
    float pi_cpu_usage;
    float pi_mem_usage;
    uint8_t current_mode;
    uint32_t sample_rate;
} system_info_t;

usb_handle_t *usb_init(const char *port);
void usb_close(usb_handle_t *h);
int usb_send_command(usb_handle_t *h, uint8_t cmd, const uint8_t *params, int param_len);
int usb_read_response(usb_handle_t *h, uint8_t *buf, int max_len);

void system_info_init(system_info_t *info);
int system_info_update(system_info_t *info, usb_handle_t *usb);

int system_set_mode(system_info_t *info, usb_handle_t *usb, uint8_t mode);
int system_set_sample_rate(system_info_t *info, usb_handle_t *usb, uint32_t rate);
int system_start_sample(system_info_t *info, usb_handle_t *usb);
int system_stop_sample(system_info_t *info, usb_handle_t *usb);
int system_set_overclock(usb_handle_t *usb, uint8_t enable);

const char *mode_to_string(uint8_t mode);

#endif