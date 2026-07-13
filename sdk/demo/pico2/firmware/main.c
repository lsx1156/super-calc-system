/**
 * @file main.c
 * @brief 超采集算系统 Demo - Pico2协处理器固件
 *        单组验证版：1片RP2350 + 8片RP2040
 * 
 * Demo验证目标：
 *   1. 信号采集分析（模拟+数字）
 *   2. 协议破解（侧信道数据聚合）
 *   3. 硬件安全测试（电压毛刺、温度、超频稳定性）
 * 
 * 职责：管理8片Pico，数据聚合，USB通信到树莓派
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/adc.h"
#include "tusb.h"
#include "pico/sem.h"

// ==================== 配置 ====================

#define NUM_PICO_SLAVES      8       // 8片Pico从设备
#define DEFAULT_FREQ_KHZ    150000  // 默认150MHz
#define OVERCLOCK_FREQ_KHZ  240000  // 超频240MHz

// SPI配置（主机）
#define SPI_PORT            spi1
#define SPI_MOSI_PIN        11
#define SPI_MISO_PIN        12
#define SPI_SCK_PIN         10
#define SPI_CS_BASE_PIN     2       // CS引脚GPIO2-9（8个从设备）
#define SPI_BAUDRATE        20000000  // 20Mbps

// USB CDC配置
#define USB_TX_BUFFER_SIZE  16384
#define USB_RX_BUFFER_SIZE  4096

// 帧格式
#define FRAME_HEADER_PICO   0xAA
#define FRAME_TAIL_PICO     0x55
#define FRAME_HEADER_USB    0x55
#define FRAME_TAIL_USB      0xAA

// 命令码（Pico）
#define CMD_NOP             0x00
#define CMD_GET_STATUS      0x01
#define CMD_START_SAMPLE    0x02
#define CMD_STOP_SAMPLE     0x03
#define CMD_SET_RATE        0x04
#define CMD_GET_DATA        0x05
#define CMD_SET_MODE        0x06
#define CMD_OVERCLOCK       0x08
#define CMD_HW_TEST         0x09
#define CMD_GLITCH          0x0A

// 命令码（USB/树莓派）
#define USB_CMD_START       0x01
#define USB_CMD_STOP        0x02
#define USB_CMD_SET_RATE    0x03
#define USB_CMD_SET_MODE    0x04
#define USB_CMD_OVERCLOCK   0x05
#define USB_CMD_GET_STATUS  0x06
#define USB_CMD_HW_TEST     0x07
#define USB_CMD_GLITCH      0x08

// 数据类型
#define DATA_ANALOG         0x01
#define DATA_DIGITAL        0x02
#define DATA_STATUS         0x03
#define DATA_HW_TEST        0x04
#define DATA_AGGREGATED     0x10  // 聚合数据

// 工作模式
#define MODE_SAMPLE         0x00
#define MODE_CRACK          0x01
#define MODE_HW_TEST        0x02

// ==================== 全局状态 ====================

typedef struct {
    uint8_t work_mode;
    uint8_t run_status;
    uint32_t sample_rate;
    uint32_t total_samples;
    uint32_t overclock_freq;
    uint8_t pico_online[NUM_PICO_SLAVES];
    uint8_t error_count;
    float temperature;
} pico2_status_t;

static pico2_status_t g_status = {0};

// 聚合缓冲区
static uint8_t aggregate_buf[USB_TX_BUFFER_SIZE];
static uint32_t aggregate_len = 0;
static semaphore_t g_aggregate_mutex;

// USB接收缓冲区
static uint8_t usb_rx_buf[USB_RX_BUFFER_SIZE];
static uint32_t usb_rx_len = 0;

// USB发送帧缓冲区（静态分配避免栈溢出）
static uint8_t usb_tx_frame[USB_TX_BUFFER_SIZE + 16];

// SPI互斥锁
static semaphore_t g_spi_mutex;

// ==================== CRC ====================

static uint16_t crc16(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

static uint32_t crc32(const uint8_t *data, uint32_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return ~crc;
}

// ==================== SPI主机 ====================

static void spi_master_init(void) {
    spi_init(SPI_PORT, SPI_BAUDRATE);
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    
    gpio_set_function(SPI_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MISO_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_SCK_PIN, GPIO_FUNC_SPI);
    
    // 初始化CS引脚
    for (int i = 0; i < NUM_PICO_SLAVES; i++) {
        gpio_init(SPI_CS_BASE_PIN + i);
        gpio_set_dir(SPI_CS_BASE_PIN + i, GPIO_OUT);
        gpio_put(SPI_CS_BASE_PIN + i, 1);
    }
    
    // 初始化SPI互斥锁
    sem_init(&g_spi_mutex, 1, 1);
    
    // 初始化聚合缓冲区互斥锁
    sem_init(&g_aggregate_mutex, 1, 1);
}

static void spi_select(uint8_t slave_id) {
    if (slave_id < NUM_PICO_SLAVES) {
        gpio_put(SPI_CS_BASE_PIN + slave_id, 0);
    }
}

static void spi_deselect(uint8_t slave_id) {
    if (slave_id < NUM_PICO_SLAVES) {
        gpio_put(SPI_CS_BASE_PIN + slave_id, 1);
    }
}

static bool spi_send_command(uint8_t slave_id, uint8_t cmd, 
                             const uint8_t *params, uint8_t param_len,
                             uint8_t *resp_buf, uint16_t *resp_len) {
    if (slave_id >= NUM_PICO_SLAVES) return false;
    
    sem_acquire_blocking(&g_spi_mutex);
    
    uint8_t tx_buf[64];
    uint8_t rx_buf[64];
    
    // 构建命令帧
    tx_buf[0] = FRAME_HEADER_PICO;
    tx_buf[1] = (1 + param_len) & 0xFF;
    tx_buf[2] = ((1 + param_len) >> 8) & 0xFF;
    tx_buf[3] = cmd;
    if (params && param_len > 0) {
        memcpy(&tx_buf[4], params, param_len);
    }
    uint16_t data_len = 1 + param_len;
    uint16_t crc = crc16(&tx_buf[3], data_len);
    tx_buf[4 + param_len] = crc & 0xFF;
    tx_buf[5 + param_len] = (crc >> 8) & 0xFF;
    tx_buf[6 + param_len] = FRAME_TAIL_PICO;
    
    uint16_t frame_len = 7 + param_len;
    
    // 发送并接收
    spi_select(slave_id);
    sleep_us(1);
    int written = spi_write_read_blocking(SPI_PORT, tx_buf, rx_buf, frame_len + 32);
    sleep_us(1);
    spi_deselect(slave_id);
    
    sem_release(&g_spi_mutex);
    
    // 解析响应
    if (rx_buf[0] == FRAME_HEADER_PICO) {
        uint16_t resp_data_len = rx_buf[1] | (rx_buf[2] << 8);
        if (resp_buf && resp_len) {
            *resp_len = resp_data_len;
            memcpy(resp_buf, &rx_buf[3], resp_data_len);
        }
        return true;
    }
    
    return false;
}

// ==================== 数据聚合 ====================

static void aggregate_data(void) {
    uint8_t resp[256];
    uint16_t resp_len = 0;
    uint32_t offset = 0;
    
    uint8_t temp_buf[USB_TX_BUFFER_SIZE];
    
    temp_buf[offset++] = DATA_AGGREGATED;
    temp_buf[offset++] = NUM_PICO_SLAVES;
    
    for (int i = 0; i < NUM_PICO_SLAVES; i++) {
        if (!g_status.pico_online[i]) continue;
        
        if (spi_send_command(i, CMD_GET_DATA, NULL, 0, resp, &resp_len)) {
            temp_buf[offset++] = i;
            temp_buf[offset++] = resp_len & 0xFF;
            temp_buf[offset++] = (resp_len >> 8) & 0xFF;
            memcpy(&temp_buf[offset], resp, resp_len);
            offset += resp_len;
        }
    }
    
    sem_acquire_blocking(&g_aggregate_mutex);
    memcpy(aggregate_buf, temp_buf, offset);
    aggregate_len = offset;
    sem_release(&g_aggregate_mutex);
    
    g_status.total_samples++;
}

// ==================== USB CDC ====================

static void usb_send_data(const uint8_t *data, uint32_t len) {
    // 构建USB帧（使用静态缓冲区避免栈溢出）
    uint8_t* frame = usb_tx_frame;
    frame[0] = FRAME_HEADER_USB;
    frame[1] = 0;  // 节点ID，0表示聚合
    frame[2] = len & 0xFF;
    frame[3] = (len >> 8) & 0xFF;
    memcpy(&frame[4], data, len);
    uint32_t crc = crc32(data, len);
    frame[4 + len] = crc & 0xFF;
    frame[5 + len] = (crc >> 8) & 0xFF;
    frame[6 + len] = (crc >> 16) & 0xFF;
    frame[7 + len] = (crc >> 24) & 0xFF;
    frame[8 + len] = FRAME_TAIL_USB;
    
    uint32_t frame_len = 9 + len;
    
    if (tud_cdc_connected()) {
        tud_cdc_write(frame, frame_len);
        tud_cdc_write_flush();
    }
}

static void usb_handle_command(uint8_t cmd, const uint8_t *params, uint8_t param_len) {
    uint8_t resp[256];
    uint16_t resp_len = 0;
    
    switch (cmd) {
        case USB_CMD_START:
            // 启动所有Pico采样
            for (int i = 0; i < NUM_PICO_SLAVES; i++) {
                if (g_status.pico_online[i]) {
                    spi_send_command(i, CMD_START_SAMPLE, NULL, 0, NULL, NULL);
                }
            }
            g_status.run_status = 1;
            resp[0] = 0x01;
            resp_len = 1;
            break;
            
        case USB_CMD_STOP:
            // 停止所有Pico采样
            for (int i = 0; i < NUM_PICO_SLAVES; i++) {
                if (g_status.pico_online[i]) {
                    spi_send_command(i, CMD_STOP_SAMPLE, NULL, 0, NULL, NULL);
                }
            }
            g_status.run_status = 0;
            resp[0] = 0x01;
            resp_len = 1;
            break;
            
        case USB_CMD_SET_RATE: {
            // 设置采样率
            uint32_t rate = params[0] | (params[1] << 8) 
                          | (params[2] << 16) | (params[3] << 24);
            g_status.sample_rate = rate;
            uint8_t rate_buf[4];
            rate_buf[0] = rate & 0xFF;
            rate_buf[1] = (rate >> 8) & 0xFF;
            rate_buf[2] = (rate >> 16) & 0xFF;
            rate_buf[3] = (rate >> 24) & 0xFF;
            for (int i = 0; i < NUM_PICO_SLAVES; i++) {
                if (g_status.pico_online[i]) {
                    spi_send_command(i, CMD_SET_RATE, rate_buf, 4, NULL, NULL);
                }
            }
            resp[0] = 0x01;
            resp_len = 1;
            break;
        }
            
        case USB_CMD_SET_MODE:
            g_status.work_mode = params[0];
            for (int i = 0; i < NUM_PICO_SLAVES; i++) {
                if (g_status.pico_online[i]) {
                    spi_send_command(i, CMD_SET_MODE, &params[0], 1, NULL, NULL);
                }
            }
            resp[0] = 0x01;
            resp_len = 1;
            break;
            
        case USB_CMD_OVERCLOCK: {
            uint8_t mode = params[0];
            if (mode == 1) {
                vreg_set_voltage(VREG_VOLTAGE_1_20);
                sleep_ms(10);
                set_sys_clock_khz(OVERCLOCK_FREQ_KHZ, true);
                g_status.overclock_freq = OVERCLOCK_FREQ_KHZ;
            } else {
                set_sys_clock_khz(DEFAULT_FREQ_KHZ, true);
                vreg_set_voltage(VREG_VOLTAGE_DEFAULT);
                g_status.overclock_freq = DEFAULT_FREQ_KHZ;
            }
            // 同时控制Pico
            for (int i = 0; i < NUM_PICO_SLAVES; i++) {
                if (g_status.pico_online[i]) {
                    spi_send_command(i, CMD_OVERCLOCK, &mode, 1, NULL, NULL);
                }
            }
            resp[0] = 0x01;
            resp_len = 1;
            break;
        }
            
        case USB_CMD_GET_STATUS:
            // 返回状态
            memcpy(resp, &g_status, sizeof(pico2_status_t));
            resp_len = sizeof(pico2_status_t);
            break;
            
        case USB_CMD_HW_TEST: {
            // 硬件安全测试
            uint8_t test_type = params[0];
            resp[0] = 0x01;
            
            switch (test_type) {
                case 0: { // 电压测试
                    float vcore = vreg_get_voltage();
                    memcpy(&resp[1], &vcore, sizeof(float));
                    resp_len = 1 + sizeof(float);
                    break;
                }
                case 1: { // 时钟频率
                    uint32_t freq = clock_get_hz(clk_sys);
                    memcpy(&resp[1], &freq, sizeof(uint32_t));
                    resp_len = 1 + sizeof(uint32_t);
                    break;
                }
                case 2: { // Pico批量测试
                    resp[1] = NUM_PICO_SLAVES;
                    resp_len = 2;
                    break;
                }
                default:
                    resp_len = 1;
                    break;
            }
            break;
        }
            
        case USB_CMD_GLITCH: {
            // 电压毛刺控制（Demo，需要外部电路）
            uint16_t width = params[0] | (params[1] << 8);
            uint8_t target = params[2];  // 目标Pico ID
            if (target < NUM_PICO_SLAVES && g_status.pico_online[target]) {
                uint8_t glitch_params[2];
                glitch_params[0] = params[0];
                glitch_params[1] = params[1];
                spi_send_command(target, CMD_GLITCH, glitch_params, 2, NULL, NULL);
            }
            resp[0] = 0x01;
            resp_len = 1;
            break;
        }
            
        default:
            resp[0] = 0xFF;
            resp_len = 1;
            break;
    }
    
    usb_send_data(resp, resp_len);
}

// ==================== Pico在线检测 ====================

static void detect_picos(void) {
    uint8_t resp[64];
    uint16_t resp_len = 0;
    
    for (int i = 0; i < NUM_PICO_SLAVES; i++) {
        if (spi_send_command(i, CMD_GET_STATUS, NULL, 0, resp, &resp_len)) {
            g_status.pico_online[i] = 1;
        } else {
            g_status.pico_online[i] = 0;
        }
        sleep_ms(1);
    }
}

// ==================== 核心0：USB + 控制 ====================

static void core0_task(void) {
    tusb_init();
    
    while (1) {
        tud_task();
        
        // 处理USB接收
        if (tud_cdc_available()) {
            uint32_t count = tud_cdc_read(usb_rx_buf, USB_RX_BUFFER_SIZE);
            if (count > 0 && usb_rx_buf[0] == FRAME_HEADER_USB) {
                uint16_t data_len = usb_rx_buf[2] | (usb_rx_buf[3] << 8);
                uint8_t cmd = usb_rx_buf[4];
                usb_handle_command(cmd, &usb_rx_buf[5], data_len - 1);
            }
        }
        
        // 运行时发送聚合数据（数据由core1聚合）
        if (g_status.run_status) {
            sem_acquire_blocking(&g_aggregate_mutex);
            uint32_t len = aggregate_len;
            uint8_t temp_data[USB_TX_BUFFER_SIZE];
            memcpy(temp_data, aggregate_buf, len);
            sem_release(&g_aggregate_mutex);
            usb_send_data(temp_data, len);
        }
        
        sleep_ms(10);
    }
}

// ==================== 核心1：SPI + 数据采集 ====================

static void core1_task(void) {
    while (1) {
        if (g_status.run_status) {
            // 持续轮询Pico数据
            aggregate_data();
            sleep_ms(1);  // ~1kHz聚合速率
        } else {
            sleep_ms(10);
        }
    }
}

// ==================== 主函数 ====================

int main(void) {
    // 初始化系统时钟
    set_sys_clock_khz(DEFAULT_FREQ_KHZ, true);
    
    // 初始化状态
    g_status.work_mode = MODE_SAMPLE;
    g_status.run_status = 0;
    g_status.sample_rate = 50000;
    g_status.total_samples = 0;
    g_status.overclock_freq = DEFAULT_FREQ_KHZ;
    g_status.error_count = 0;
    g_status.temperature = 25.0f;
    
    // 初始化SPI
    spi_master_init();
    
    // 检测Pico
    detect_picos();
    
    // LED指示
    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);
    
    // 启动双核
    multicore_launch_core1(core1_task);
    
    // 核心0运行USB和控制
    core0_task();
    
    return 0;
}