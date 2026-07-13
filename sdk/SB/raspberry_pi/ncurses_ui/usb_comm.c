#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <time.h>

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

static int serial_open(const char *port, int baudrate);
static void serial_close(int fd, struct termios *oldtio);
static int serial_read(int fd, uint8_t *buf, int len);
static int serial_write(int fd, const uint8_t *buf, int len);
static float read_pi_temp(void);
static float read_pi_cpu_usage(void);
static float read_pi_mem_usage(void);

usb_handle_t *usb_init(const char *port) {
    usb_handle_t *h = malloc(sizeof(usb_handle_t));
    if (!h) return NULL;
    memset(h, 0, sizeof(usb_handle_t));
    
    h->fd = serial_open(port ? port : USB_PORT, USB_BAUDRATE);
    if (h->fd < 0) {
        free(h);
        return NULL;
    }
    
    h->rx_len = 0;
    return h;
}

void usb_close(usb_handle_t *h) {
    if (!h) return;
    serial_close(h->fd, &h->oldtio);
    free(h);
}

static int serial_open(const char *port, int baudrate) {
    int fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) return -1;
    
    struct termios newtio;
    tcgetattr(fd, &newtio);
    
    cfsetispeed(&newtio, B115200);
    cfsetospeed(&newtio, B115200);
    
    newtio.c_cflag |= (CLOCAL | CREAD);
    newtio.c_cflag &= ~PARENB;
    newtio.c_cflag &= ~CSTOPB;
    newtio.c_cflag &= ~CSIZE;
    newtio.c_cflag |= CS8;
    
    newtio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    newtio.c_iflag &= ~(IXON | IXOFF | IXANY);
    newtio.c_oflag &= ~OPOST;
    
    newtio.c_cc[VMIN] = 0;
    newtio.c_cc[VTIME] = 1;
    
    tcflush(fd, TCIOFLUSH);
    tcsetattr(fd, TCSANOW, &newtio);
    
    return fd;
}

static void serial_close(int fd, struct termios *oldtio) {
    tcsetattr(fd, TCSANOW, oldtio);
    close(fd);
}

static int serial_read(int fd, uint8_t *buf, int len) {
    return read(fd, buf, len);
}

static int serial_write(int fd, const uint8_t *buf, int len) {
    return write(fd, buf, len);
}

int usb_send_command(usb_handle_t *h, uint8_t cmd, const uint8_t *params, int param_len) {
    if (!h || h->fd < 0) return -1;
    
    uint8_t frame[256];
    int frame_len = 0;
    
    frame[frame_len++] = FRAME_HEADER;
    frame[frame_len++] = 0;
    frame[frame_len++] = (1 + param_len) & 0xFF;
    frame[frame_len++] = ((1 + param_len) >> 8) & 0xFF;
    frame[frame_len++] = cmd;
    
    if (params && param_len > 0) {
        memcpy(&frame[frame_len], params, param_len);
        frame_len += param_len;
    }
    
    uint32_t crc = 0;
    for (int i = 4; i < frame_len; i++) {
        crc ^= frame[i];
    }
    frame[frame_len++] = crc & 0xFF;
    frame[frame_len++] = (crc >> 8) & 0xFF;
    frame[frame_len++] = (crc >> 16) & 0xFF;
    frame[frame_len++] = (crc >> 24) & 0xFF;
    frame[frame_len++] = FRAME_TAIL;
    
    return serial_write(h->fd, frame, frame_len);
}

int usb_read_response(usb_handle_t *h, uint8_t *buf, int max_len) {
    if (!h || h->fd < 0) return -1;
    
    uint8_t temp[256];
    int n = serial_read(h->fd, temp, sizeof(temp));
    if (n <= 0) return 0;
    
    memcpy(&h->rx_buffer[h->rx_len], temp, n);
    h->rx_len += n;
    
    if (h->rx_len < 6) return 0;
    
    int idx = 0;
    while (idx < h->rx_len - 1) {
        if (h->rx_buffer[idx] == FRAME_HEADER && h->rx_buffer[idx + 5] == FRAME_TAIL) {
            int data_len = h->rx_buffer[idx + 2] | (h->rx_buffer[idx + 3] << 8);
            int total_len = data_len + 6;
            
            if (idx + total_len <= h->rx_len) {
                int copy_len = (total_len < max_len) ? total_len : max_len;
                memcpy(buf, &h->rx_buffer[idx], copy_len);
                memmove(h->rx_buffer, &h->rx_buffer[idx + total_len], h->rx_len - idx - total_len);
                h->rx_len -= idx + total_len;
                return data_len;
            }
        }
        idx++;
    }
    
    return 0;
}

void system_info_init(system_info_t *info) {
    if (!info) return;
    memset(info, 0, sizeof(system_info_t));
    info->pico2_count = 1;
    info->pico_count = 8;
    info->current_mode = MODE_STANDBY;
    info->sample_rate = 50000;
}

int system_info_update(system_info_t *info, usb_handle_t *usb) {
    if (!info) return -1;
    
    info->pi_temp = read_pi_temp();
    info->pi_cpu_usage = read_pi_cpu_usage();
    info->pi_mem_usage = read_pi_mem_usage();
    
    if (usb && usb->fd >= 0) {
        usb_send_command(usb, CMD_GET_STATUS, NULL, 0);
        usleep(100000);
        
        uint8_t resp[256];
        int len = usb_read_response(usb, resp, sizeof(resp));
        if (len > 0) {
            info->pico2_online = 1;
            
            if (len >= 20) {
                info->pico2_status.work_mode = resp[0];
                info->pico2_status.run_status = resp[1];
                info->pico2_status.sample_rate = resp[2] | (resp[3] << 8) | (resp[4] << 16) | (resp[5] << 24);
                info->pico2_status.total_samples = resp[6] | (resp[7] << 8) | (resp[8] << 16) | (resp[9] << 24);
                info->pico2_status.overclock_freq = resp[10] | (resp[11] << 8) | (resp[12] << 16) | (resp[13] << 24);
                info->pico2_status.core_temp = (float)(resp[14] | (resp[15] << 8)) / 100.0f;
                info->pico2_status.vcore = (float)(resp[16] | (resp[17] << 8)) / 1000.0f;
                info->pico2_status.error_count = resp[18] | (resp[19] << 8);
                info->current_mode = resp[0];
                info->sample_rate = info->pico2_status.sample_rate;
            }
        } else {
            info->pico2_online = 0;
        }
    }
    
    info->pico_online = info->pico2_online ? info->pico_count : 0;
    
    return 0;
}

static float read_pi_temp(void) {
    FILE *f = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (!f) return 0.0f;
    
    int temp;
    fscanf(f, "%d", &temp);
    fclose(f);
    
    return (float)temp / 1000.0f;
}

static float read_pi_cpu_usage(void) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return 0.0f;
    
    unsigned long long user, nice, system, idle;
    fscanf(f, "cpu %llu %llu %llu %llu", &user, &nice, &system, &idle);
    fclose(f);
    
    static unsigned long long last_user, last_nice, last_system, last_idle;
    unsigned long long total = user + nice + system + idle;
    unsigned long long delta_total = total - (last_user + last_nice + last_system + last_idle);
    unsigned long long delta_idle = idle - last_idle;
    
    last_user = user;
    last_nice = nice;
    last_system = system;
    last_idle = idle;
    
    if (delta_total == 0) return 0.0f;
    
    return 100.0f * (1.0f - (float)delta_idle / (float)delta_total);
}

static float read_pi_mem_usage(void) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return 0.0f;
    
    unsigned long total, free, available;
    char line[64];
    
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "MemTotal: %lu kB", &total) == 1) continue;
        if (sscanf(line, "MemFree: %lu kB", &free) == 1) continue;
        if (sscanf(line, "MemAvailable: %lu kB", &available) == 1) break;
    }
    fclose(f);
    
    if (total == 0) return 0.0f;
    
    return 100.0f * (1.0f - (float)available / (float)total);
}

int system_set_mode(system_info_t *info, usb_handle_t *usb, uint8_t mode) {
    if (!info || !usb || usb->fd < 0) return -1;
    
    uint8_t param = mode;
    usb_send_command(usb, CMD_SET_MODE, &param, 1);
    info->current_mode = mode;
    
    return 0;
}

int system_set_sample_rate(system_info_t *info, usb_handle_t *usb, uint32_t rate) {
    if (!info || !usb || usb->fd < 0) return -1;
    
    uint8_t params[4];
    params[0] = rate & 0xFF;
    params[1] = (rate >> 8) & 0xFF;
    params[2] = (rate >> 16) & 0xFF;
    params[3] = (rate >> 24) & 0xFF;
    
    usb_send_command(usb, CMD_SET_RATE, params, 4);
    info->sample_rate = rate;
    
    return 0;
}

int system_start_sample(system_info_t *info, usb_handle_t *usb) {
    if (!info || !usb || usb->fd < 0) return -1;
    usb_send_command(usb, CMD_START_SAMPLE, NULL, 0);
    info->pico2_status.run_status = 1;
    return 0;
}

int system_stop_sample(system_info_t *info, usb_handle_t *usb) {
    if (!info || !usb || usb->fd < 0) return -1;
    usb_send_command(usb, CMD_STOP_SAMPLE, NULL, 0);
    info->pico2_status.run_status = 0;
    return 0;
}

int system_set_overclock(usb_handle_t *usb, uint8_t enable) {
    if (!usb || usb->fd < 0) return -1;
    usb_send_command(usb, CMD_OVERCLOCK, &enable, 1);
    return 0;
}

const char *mode_to_string(uint8_t mode) {
    switch (mode) {
        case MODE_SAMPLE: return "Sample";
        case MODE_CRACK: return "Crack";
        case MODE_BRUTEFORCE: return "BruteForce";
        case MODE_HW_TEST: return "HW Test";
        case MODE_STANDBY: return "Standby";
        default: return "Unknown";
    }
}