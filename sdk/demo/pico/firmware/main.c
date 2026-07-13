/**
 * @file main.c
 * @brief 超采集算系统 Demo - Pico终端芯片固件
 *        单组验证版：8片Pico，每片4路ADC + 8路数字捕获
 * 
 * Demo验证目标：
 *   1. 信号采集分析（模拟+数字）
 *   2. 协议破解（侧信道数据采集）
 *   3. 硬件安全测试（电压毛刺、温度测试）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"

// ==================== 配置 ====================

#define NODE_ID_DEFAULT     0
#define DEFAULT_FREQ_KHZ    133000  // 默认133MHz
#define OVERCLOCK_FREQ_KHZ  200000  // 超频200MHz

// ADC配置
#define ADC_CHANNELS        4       // 4路ADC
#define ADC_BUFFER_SIZE     512     // 采样缓冲区

// 数字捕获配置
#define DIGITAL_CHANNELS    8       // 8路数字
#define DIGITAL_BUFFER_SIZE 1024    // 捕获缓冲区

// SPI配置（从机）
#define SPI_PORT            spi0
#define SPI_MOSI_PIN        16
#define SPI_MISO_PIN        19
#define SPI_SCK_PIN         18
#define SPI_CS_PIN          17
#define SPI_BAUDRATE        20000000  // 20Mbps

// 数字捕获引脚起始位置（避开SPI和其他占用引脚）
#define DIGITAL_PIN_START   6

// 帧格式
#define FRAME_HEADER        0xAA
#define FRAME_TAIL          0x55

// 命令码
#define CMD_NOP             0x00
#define CMD_GET_STATUS      0x01
#define CMD_START_SAMPLE    0x02
#define CMD_STOP_SAMPLE     0x03
#define CMD_SET_RATE        0x04
#define CMD_GET_DATA        0x05
#define CMD_SET_MODE        0x06
#define CMD_OVERCLOCK       0x08
#define CMD_HW_TEST         0x09  // 硬件安全测试
#define CMD_GLITCH          0x0A  // 电压毛刺注入

// 数据类型
#define DATA_ANALOG         0x01
#define DATA_DIGITAL        0x02
#define DATA_STATUS         0x03
#define DATA_HW_TEST        0x04  // 硬件测试数据

// 工作模式
#define MODE_SAMPLE         0x00  // 采样模式
#define MODE_CRACK          0x01  // 破译模式（侧信道采集）
#define MODE_HW_TEST        0x02  // 硬件安全测试

// ==================== 全局状态 ====================

typedef struct {
    uint8_t node_id;
    uint8_t work_mode;       // 0=采样, 1=破译, 2=硬件测试
    uint8_t run_status;      // 0=停止, 1=运行
    uint32_t adc_rate;       // ADC采样率
    uint32_t digital_rate;   // 数字捕获率
    uint32_t sample_count;   // 采样计数
    uint32_t overclock_freq; // 超频频率
    uint8_t error_code;      // 错误码
    float temperature;       // 温度
} node_status_t;

static node_status_t g_status = {0};

// ADC缓冲区
static uint16_t adc_buffer[ADC_BUFFER_SIZE];
static volatile uint32_t adc_write_idx = 0;
static volatile bool adc_running = false;

// 数字捕获缓冲区
static uint8_t digital_buffer[DIGITAL_BUFFER_SIZE];
static volatile uint32_t digital_write_idx = 0;
static volatile bool digital_running = false;

// SPI缓冲区
static uint8_t spi_tx_buf[2048];
static uint8_t spi_rx_buf[2048];

// ==================== CRC16 ====================

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

// ==================== ADC ====================

static void adc_init_hw(void) {
    adc_init();
    for (int i = 0; i < ADC_CHANNELS; i++) {
        adc_gpio_init(26 + i);  // GPIO26-29
    }
    adc_set_temp_sensor_enabled(true);
}

static void adc_set_sample_rate(uint32_t rate) {
    if (rate > 125000) rate = 125000;
    // 简化时钟分频设置
    adc_set_clkdiv(48000000.0f / (rate * 96.0f));
}

static void adc_start(void) {
    adc_run(false);
    adc_fifo_setup(true, false, 1, false, false);
    adc_set_round_robin(0x0F);  // 4路轮询
    adc_run(true);
    adc_running = true;
}

static void adc_stop(void) {
    adc_run(false);
    adc_fifo_drain();
    adc_running = false;
}

static float adc_read_temp(void) {
    adc_set_temp_sensor_enabled(true);
    sleep_ms(10);
    uint16_t raw = adc_read();
    float voltage = raw * 3.3f / 4095.0f;
    return 27.0f - (voltage - 0.706f) / 0.001721f;
}

// ==================== 数字捕获 ====================

static void digital_init_hw(void) {
    for (int i = 0; i < DIGITAL_CHANNELS; i++) {
        gpio_init(DIGITAL_PIN_START + i);
        gpio_set_dir(DIGITAL_PIN_START + i, GPIO_IN);
        gpio_pull_down(DIGITAL_PIN_START + i);
    }
}

static uint8_t digital_read_all(void) {
    uint8_t val = 0;
    for (int i = 0; i < DIGITAL_CHANNELS; i++) {
        val |= (gpio_get(DIGITAL_PIN_START + i) ? 1 : 0) << i;
    }
    return val;
}

// ==================== SPI从机 ====================

static void spi_slave_init(void) {
    spi_init(SPI_PORT, SPI_BAUDRATE);
    spi_set_slave(SPI_PORT, true);
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    
    gpio_set_function(SPI_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MISO_PIN, GPIO_FUNC_SPI);
    
    // CS引脚使用GPIO模式而非SPI功能，以便能够读取CS状态
    gpio_init(SPI_CS_PIN);
    gpio_set_dir(SPI_CS_PIN, GPIO_IN);
    gpio_pull_up(SPI_CS_PIN);
}

static void spi_send_response(const uint8_t *data, uint16_t len) {
    // 构建响应帧
    uint8_t frame[2048];
    frame[0] = FRAME_HEADER;
    frame[1] = len & 0xFF;
    frame[2] = (len >> 8) & 0xFF;
    memcpy(&frame[3], data, len);
    uint16_t crc = crc16(data, len);
    frame[3 + len] = crc & 0xFF;
    frame[4 + len] = (crc >> 8) & 0xFF;
    frame[5 + len] = FRAME_TAIL;
    
    // 发送
    spi_write_blocking(SPI_PORT, frame, 6 + len);
}

static void spi_handle_command(const uint8_t *cmd_frame) {
    // 验证帧头
    if (cmd_frame[0] != FRAME_HEADER) return;
    
    uint16_t data_len = cmd_frame[1] | (cmd_frame[2] << 8);
    uint8_t cmd = cmd_frame[3];
    
    uint8_t resp_data[512];
    uint16_t resp_len = 0;
    
    switch (cmd) {
        case CMD_NOP:
            resp_data[0] = 0x00;
            resp_len = 1;
            break;
            
        case CMD_GET_STATUS:
            // 返回节点状态
            g_status.temperature = adc_read_temp();
            memcpy(resp_data, &g_status, sizeof(node_status_t));
            resp_len = sizeof(node_status_t);
            break;
            
        case CMD_START_SAMPLE:
            g_status.run_status = 1;
            adc_start();
            digital_running = true;
            resp_data[0] = 0x01;
            resp_len = 1;
            break;
            
        case CMD_STOP_SAMPLE:
            g_status.run_status = 0;
            adc_stop();
            digital_running = false;
            resp_data[0] = 0x01;
            resp_len = 1;
            break;
            
        case CMD_SET_RATE:
            g_status.adc_rate = cmd_frame[4] | (cmd_frame[5] << 8) 
                              | (cmd_frame[6] << 16) | (cmd_frame[7] << 24);
            adc_set_sample_rate(g_status.adc_rate);
            resp_data[0] = 0x01;
            resp_len = 1;
            break;
            
        case CMD_GET_DATA: {
            // 返回ADC + 数字数据
            resp_data[0] = DATA_ANALOG;
            // 模拟数据：每通道采样一次
            for (int i = 0; i < ADC_CHANNELS; i++) {
                adc_select_input(i);
                uint16_t val = adc_read();
                resp_data[1 + i*2] = val & 0xFF;
                resp_data[2 + i*2] = (val >> 8) & 0xFF;
            }
            // 数字数据
            resp_data[1 + ADC_CHANNELS*2] = DATA_DIGITAL;
            resp_data[2 + ADC_CHANNELS*2] = digital_read_all();
            resp_len = 3 + ADC_CHANNELS*2;
            g_status.sample_count++;
            break;
        }
            
        case CMD_SET_MODE:
            g_status.work_mode = cmd_frame[4];
            resp_data[0] = 0x01;
            resp_len = 1;
            break;
            
        case CMD_OVERCLOCK: {
            uint8_t mode = cmd_frame[4];
            if (mode == 1) {
                // 超频
                vreg_set_voltage(VREG_VOLTAGE_1_20);
                sleep_ms(10);
                set_sys_clock_khz(OVERCLOCK_FREQ_KHZ, true);
                g_status.overclock_freq = OVERCLOCK_FREQ_KHZ;
            } else {
                // 默认
                set_sys_clock_khz(DEFAULT_FREQ_KHZ, true);
                vreg_set_voltage(VREG_VOLTAGE_DEFAULT);
                g_status.overclock_freq = DEFAULT_FREQ_KHZ;
            }
            resp_data[0] = 0x01;
            resp_len = 1;
            break;
        }
            
        case CMD_HW_TEST: {
            // 硬件安全测试
            uint8_t test_type = cmd_frame[4];
            resp_data[0] = 0x01;
            
            switch (test_type) {
                case 0: { // 电压监控
                    float vcore = vreg_get_voltage();
                    memcpy(&resp_data[1], &vcore, sizeof(float));
                    resp_len = 1 + sizeof(float);
                    break;
                }
                case 1: { // 温度测试
                    float temp = adc_read_temp();
                    memcpy(&resp_data[1], &temp, sizeof(float));
                    resp_len = 1 + sizeof(float);
                    break;
                }
                case 2: { // 时钟频率测量
                    uint32_t freq = clock_get_hz(clk_sys);
                    memcpy(&resp_data[1], &freq, sizeof(uint32_t));
                    resp_len = 1 + sizeof(uint32_t);
                    break;
                }
                default:
                    resp_len = 1;
                    break;
            }
            break;
        }
            
        case CMD_GLITCH: {
            // 电压毛刺注入（Demo）
            // 实际硬件需要外部电路，这里模拟
            uint16_t glitch_width = cmd_frame[4] | (cmd_frame[5] << 8);
            resp_data[0] = 0x01;
            resp_len = 1;
            break;
        }
            
        default:
            resp_data[0] = 0xFF;  // 未知命令
            resp_len = 1;
            break;
    }
    
    spi_send_response(resp_data, resp_len);
}

// ==================== 主循环 ====================

static void spi_task(void) {
    uint8_t rx_byte;
    
    while (1) {
        // 检查CS引脚
        if (!gpio_get(SPI_CS_PIN)) {
            // CS选中，开始接收
            int count = spi_read_blocking(SPI_PORT, 0, spi_rx_buf, 32);
            if (count > 0 && spi_rx_buf[0] == FRAME_HEADER) {
                spi_handle_command(spi_rx_buf);
            }
        }
        sleep_us(10);
    }
}

static void sample_task(void) {
    while (1) {
        if (g_status.run_status) {
            // 采样循环
            if (adc_running) {
                // 从ADC FIFO读取
                while (!adc_fifo_is_empty()) {
                    uint16_t val = adc_fifo_get();
                    adc_buffer[adc_write_idx++ % ADC_BUFFER_SIZE] = val;
                }
            }
            
            if (digital_running) {
                digital_buffer[digital_write_idx++ % DIGITAL_BUFFER_SIZE] = digital_read_all();
            }
        }
        sleep_us(100);
    }
}

// ==================== 主函数 ====================

int main(void) {
    // 初始化系统时钟
    set_sys_clock_khz(DEFAULT_FREQ_KHZ, true);
    
    // 初始化状态
    g_status.node_id = NODE_ID_DEFAULT;
    g_status.work_mode = MODE_SAMPLE;
    g_status.run_status = 0;
    g_status.adc_rate = 50000;
    g_status.digital_rate = 50000000;
    g_status.sample_count = 0;
    g_status.overclock_freq = DEFAULT_FREQ_KHZ;
    g_status.error_code = 0;
    
    // 初始化硬件
    adc_init_hw();
    digital_init_hw();
    spi_slave_init();
    
    // LED指示
    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);
    
    // 启动双核
    multicore_launch_core1(spi_task);
    
    // 主循环
    sample_task();
    
    return 0;
}