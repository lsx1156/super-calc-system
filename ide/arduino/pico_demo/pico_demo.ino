#include <Arduino.h>
#include <SPI.h>

#define NODE_ID_DEFAULT     0
#define DEFAULT_FREQ_KHZ    133000
#define OVERCLOCK_FREQ_KHZ  200000

#define ADC_CHANNELS        4
#define ADC_BUFFER_SIZE     512

#define DIGITAL_CHANNELS    8
#define DIGITAL_BUFFER_SIZE 1024

#define SPI_PORT            0
#define SPI_MOSI_PIN        16
#define SPI_MISO_PIN        19
#define SPI_SCK_PIN         18
#define SPI_CS_PIN          17
#define SPI_BAUDRATE        20000000

#define DIGITAL_PIN_START   6

#define FRAME_HEADER        0xAA
#define FRAME_TAIL          0x55

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

#define DATA_ANALOG         0x01
#define DATA_DIGITAL        0x02
#define DATA_STATUS         0x03
#define DATA_HW_TEST        0x04

#define MODE_SAMPLE         0x00
#define MODE_CRACK          0x01
#define MODE_HW_TEST        0x02

typedef struct {
    uint8_t node_id;
    uint8_t work_mode;
    uint8_t run_status;
    uint32_t adc_rate;
    uint32_t digital_rate;
    uint32_t sample_count;
    uint32_t overclock_freq;
    uint8_t error_code;
    float temperature;
} node_status_t;

node_status_t g_status = {0};

uint16_t adc_buffer[ADC_BUFFER_SIZE];
volatile uint32_t adc_write_idx = 0;
volatile bool adc_running = false;

uint8_t digital_buffer[DIGITAL_BUFFER_SIZE];
volatile uint32_t digital_write_idx = 0;
volatile bool digital_running = false;

uint8_t spi_tx_buf[2048];
uint8_t spi_rx_buf[2048];

uint16_t crc16(const uint8_t *data, uint16_t len) {
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

void adc_init_hw(void) {
    adc_init();
    for (int i = 0; i < ADC_CHANNELS; i++) {
        adc_gpio_init(26 + i);
    }
    adc_set_temp_sensor_enabled(true);
}

void adc_set_sample_rate(uint32_t rate) {
    if (rate > 125000) rate = 125000;
    adc_set_clkdiv(48000000.0f / (rate * 96.0f));
}

void adc_start(void) {
    adc_run(false);
    adc_fifo_setup(true, false, 1, false, false);
    adc_set_round_robin(0x0F);
    adc_run(true);
    adc_running = true;
}

void adc_stop(void) {
    adc_run(false);
    adc_fifo_drain();
    adc_running = false;
}

float adc_read_temp(void) {
    adc_select_input(4);
    delayMicroseconds(10);
    uint16_t raw = adc_read();
    float voltage = raw * 3.3f / 4095.0f;
    return 27.0f - (voltage - 0.706f) / 0.001721f;
}

void digital_init_hw(void) {
    for (int i = 0; i < DIGITAL_CHANNELS; i++) {
        pinMode(DIGITAL_PIN_START + i, INPUT_PULLDOWN);
    }
}

uint8_t digital_read_all(void) {
    uint8_t val = 0;
    for (int i = 0; i < DIGITAL_CHANNELS; i++) {
        val |= (digitalRead(DIGITAL_PIN_START + i) ? 1 : 0) << i;
    }
    return val;
}

void spi_slave_init(void) {
    SPI.setRX(SPI_MISO_PIN);
    SPI.setTX(SPI_MOSI_PIN);
    SPI.setSCK(SPI_SCK_PIN);
    pinMode(SPI_CS_PIN, INPUT_PULLUP);
    
    SPI.begin();
    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE0);
    SPI.setClockDivider(SPI_CLOCK_DIV2);
}

void spi_send_response(const uint8_t *data, uint16_t len) {
    uint8_t frame[256];
    frame[0] = FRAME_HEADER;
    frame[1] = len & 0xFF;
    frame[2] = (len >> 8) & 0xFF;
    memcpy(&frame[3], data, len);
    uint16_t crc = crc16(data, len);
    frame[3 + len] = crc & 0xFF;
    frame[4 + len] = (crc >> 8) & 0xFF;
    frame[5 + len] = FRAME_TAIL;
    
    for (uint16_t i = 0; i < 6 + len; i++) {
        SPI.transfer(frame[i]);
    }
}

void spi_handle_command(const uint8_t *cmd_frame) {
    if (cmd_frame[0] != FRAME_HEADER) return;
    
    uint16_t data_len = cmd_frame[1] | (cmd_frame[2] << 8);
    uint8_t cmd = cmd_frame[3];
    
    uint8_t resp_data[256];
    uint16_t resp_len = 0;
    
    switch (cmd) {
        case CMD_NOP:
            resp_data[0] = 0x00;
            resp_len = 1;
            break;
            
        case CMD_GET_STATUS:
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
            
        case CMD_SET_RATE: {
            uint32_t rate = cmd_frame[4] | (cmd_frame[5] << 8) 
                          | (cmd_frame[6] << 16) | (cmd_frame[7] << 24);
            g_status.adc_rate = rate;
            adc_set_sample_rate(rate);
            resp_data[0] = 0x01;
            resp_len = 1;
            break;
        }
            
        case CMD_GET_DATA: {
            resp_data[0] = DATA_ANALOG;
            for (int i = 0; i < ADC_CHANNELS; i++) {
                adc_select_input(i);
                uint16_t val = adc_read();
                resp_data[1 + i*2] = val & 0xFF;
                resp_data[2 + i*2] = (val >> 8) & 0xFF;
            }
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
                set_cpu_freq(200);
                g_status.overclock_freq = OVERCLOCK_FREQ_KHZ;
            } else {
                set_cpu_freq(133);
                g_status.overclock_freq = DEFAULT_FREQ_KHZ;
            }
            resp_data[0] = 0x01;
            resp_len = 1;
            break;
        }
            
        case CMD_HW_TEST: {
            uint8_t test_type = cmd_frame[4];
            resp_data[0] = 0x01;
            
            switch (test_type) {
                case 0: {
                    float vcore = 3.3f;
                    memcpy(&resp_data[1], &vcore, sizeof(float));
                    resp_len = 1 + sizeof(float);
                    break;
                }
                case 1: {
                    float temp = adc_read_temp();
                    memcpy(&resp_data[1], &temp, sizeof(float));
                    resp_len = 1 + sizeof(float);
                    break;
                }
                case 2: {
                    uint32_t freq = SystemCoreClock;
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
            uint16_t glitch_width = cmd_frame[4] | (cmd_frame[5] << 8);
            resp_data[0] = 0x01;
            resp_len = 1;
            break;
        }
            
        default:
            resp_data[0] = 0xFF;
            resp_len = 1;
            break;
    }
    
    spi_send_response(resp_data, resp_len);
}

void spi_task(void) {
    static uint16_t rx_pos = 0;
    
    bool cs_low = !digitalRead(SPI_CS_PIN);
    
    if (cs_low) {
        while (SPI.available()) {
            if (rx_pos < sizeof(spi_rx_buf)) {
                spi_rx_buf[rx_pos++] = SPI.read();
            }
        }
    } else {
        if (rx_pos >= 7 && spi_rx_buf[0] == FRAME_HEADER && spi_rx_buf[rx_pos - 1] == FRAME_TAIL) {
            spi_handle_command(spi_rx_buf);
        }
        rx_pos = 0;
    }
}

void sample_task(void) {
    if (g_status.run_status) {
        if (adc_running) {
            while (!adc_fifo_is_empty()) {
                uint16_t val = adc_fifo_get();
                adc_buffer[adc_write_idx++ % ADC_BUFFER_SIZE] = val;
            }
        }
        
        if (digital_running) {
            digital_buffer[digital_write_idx++ % DIGITAL_BUFFER_SIZE] = digital_read_all();
        }
    }
}

void setup() {
    set_cpu_freq(133);
    
    g_status.node_id = NODE_ID_DEFAULT;
    g_status.work_mode = MODE_SAMPLE;
    g_status.run_status = 0;
    g_status.adc_rate = 50000;
    g_status.digital_rate = 50000000;
    g_status.sample_count = 0;
    g_status.overclock_freq = DEFAULT_FREQ_KHZ;
    g_status.error_code = 0;
    
    adc_init_hw();
    digital_init_hw();
    spi_slave_init();
    
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    
    delay(100);
}

void loop() {
    spi_task();
    sample_task();
    
    delayMicroseconds(10);
}