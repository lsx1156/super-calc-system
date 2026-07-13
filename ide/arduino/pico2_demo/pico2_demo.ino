#include <Arduino.h>
#include <SPI.h>

#define NUM_PICO_SLAVES      8
#define DEFAULT_FREQ_KHZ    150000
#define OVERCLOCK_FREQ_KHZ  240000

#define SPI_PORT            1
#define SPI_MOSI_PIN        11
#define SPI_MISO_PIN        12
#define SPI_SCK_PIN         10
#define SPI_CS_BASE_PIN     2
#define SPI_BAUDRATE        20000000

#define USB_BAUDRATE        100000000

#define FRAME_HEADER_PICO   0xAA
#define FRAME_TAIL_PICO     0x55
#define FRAME_HEADER_USB    0x55
#define FRAME_TAIL_USB      0xAA

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

#define USB_CMD_START       0x01
#define USB_CMD_STOP        0x02
#define USB_CMD_SET_RATE    0x03
#define USB_CMD_SET_MODE    0x04
#define USB_CMD_OVERCLOCK   0x05
#define USB_CMD_GET_STATUS  0x06
#define USB_CMD_HW_TEST     0x07
#define USB_CMD_GLITCH      0x08

#define DATA_ANALOG         0x01
#define DATA_DIGITAL        0x02
#define DATA_STATUS         0x03
#define DATA_HW_TEST        0x04
#define DATA_AGGREGATED     0x10

#define MODE_SAMPLE         0x00
#define MODE_CRACK          0x01
#define MODE_HW_TEST        0x02

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

pico2_status_t g_status = {0};

uint8_t aggregate_buf[8192];
uint32_t aggregate_len = 0;

uint8_t usb_rx_buf[512];
uint32_t usb_rx_len = 0;

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

uint32_t crc32(const uint8_t *data, uint32_t len) {
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

void spi_master_init(void) {
    SPI.setRX(SPI_MISO_PIN);
    SPI.setTX(SPI_MOSI_PIN);
    SPI.setSCK(SPI_SCK_PIN);
    
    for (int i = 0; i < NUM_PICO_SLAVES; i++) {
        pinMode(SPI_CS_BASE_PIN + i, OUTPUT);
        digitalWrite(SPI_CS_BASE_PIN + i, HIGH);
    }
    
    SPI.begin();
    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE0);
    SPI.setClockDivider(SPI_CLOCK_DIV2);
}

bool spi_send_command(uint8_t slave_id, uint8_t cmd, 
                      const uint8_t *params, uint8_t param_len,
                      uint8_t *resp_buf, uint16_t *resp_len) {
    if (slave_id >= NUM_PICO_SLAVES) return false;
    
    uint8_t tx_buf[64];
    uint8_t rx_buf[64];
    
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
    
    digitalWrite(SPI_CS_BASE_PIN + slave_id, LOW);
    delayMicroseconds(1);
    
    for (int i = 0; i < frame_len + 32; i++) {
        uint8_t tx_data = (i < frame_len) ? tx_buf[i] : 0;
        rx_buf[i] = SPI.transfer(tx_data);
    }
    
    delayMicroseconds(1);
    digitalWrite(SPI_CS_BASE_PIN + slave_id, HIGH);
    
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

void aggregate_data(void) {
    uint8_t resp[256];
    uint16_t resp_len = 0;
    uint32_t offset = 0;
    
    uint8_t temp_buf[512];
    
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
    
    memcpy(aggregate_buf, temp_buf, offset);
    aggregate_len = offset;
    
    g_status.total_samples++;
}

void usb_send_data(const uint8_t *data, uint32_t len) {
    uint8_t frame[512];
    frame[0] = FRAME_HEADER_USB;
    frame[1] = 0;
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
    
    Serial.write(frame, frame_len);
    Serial.flush();
}

void usb_handle_command(uint8_t cmd, const uint8_t *params, uint8_t param_len) {
    uint8_t resp[256];
    uint16_t resp_len = 0;
    
    switch (cmd) {
        case USB_CMD_START:
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
                set_cpu_freq(240);
                g_status.overclock_freq = OVERCLOCK_FREQ_KHZ;
            } else {
                set_cpu_freq(150);
                g_status.overclock_freq = DEFAULT_FREQ_KHZ;
            }
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
            memcpy(resp, &g_status, sizeof(pico2_status_t));
            resp_len = sizeof(pico2_status_t);
            break;
            
        case USB_CMD_HW_TEST: {
            uint8_t test_type = params[0];
            resp[0] = 0x01;
            
            switch (test_type) {
                case 0: {
                    float vcore = 3.3f;
                    memcpy(&resp[1], &vcore, sizeof(float));
                    resp_len = 1 + sizeof(float);
                    break;
                }
                case 1: {
                    uint32_t freq = SystemCoreClock;
                    memcpy(&resp[1], &freq, sizeof(uint32_t));
                    resp_len = 1 + sizeof(uint32_t);
                    break;
                }
                case 2: {
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
            uint16_t width = params[0] | (params[1] << 8);
            uint8_t target = params[2];
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

void detect_picos(void) {
    uint8_t resp[64];
    uint16_t resp_len = 0;
    
    for (int i = 0; i < NUM_PICO_SLAVES; i++) {
        if (spi_send_command(i, CMD_GET_STATUS, NULL, 0, resp, &resp_len)) {
            g_status.pico_online[i] = 1;
        } else {
            g_status.pico_online[i] = 0;
        }
        delayMicroseconds(1);
    }
}

void setup() {
    set_cpu_freq(150);
    
    g_status.work_mode = MODE_SAMPLE;
    g_status.run_status = 0;
    g_status.sample_rate = 50000;
    g_status.total_samples = 0;
    g_status.overclock_freq = DEFAULT_FREQ_KHZ;
    g_status.error_count = 0;
    g_status.temperature = 25.0f;
    
    spi_master_init();
    
    Serial.begin(USB_BAUDRATE);
    
    detect_picos();
    
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    
    adc_init();
    adc_set_temp_sensor_enabled(true);
    
    delay(500);
}

void loop() {
    while (Serial.available() > 0) {
        if (usb_rx_len < sizeof(usb_rx_buf)) {
            usb_rx_buf[usb_rx_len++] = Serial.read();
        }
    }
    
    if (usb_rx_len >= 9) {
        if (usb_rx_buf[0] == FRAME_HEADER_USB && usb_rx_buf[usb_rx_len - 1] == FRAME_TAIL_USB) {
            uint16_t data_len = usb_rx_buf[2] | (usb_rx_buf[3] << 8);
            uint8_t cmd = usb_rx_buf[4];
            usb_handle_command(cmd, &usb_rx_buf[5], data_len - 1);
        }
        usb_rx_len = 0;
    }
    
    if (g_status.run_status) {
        aggregate_data();
        usb_send_data(aggregate_buf, aggregate_len);
    }
    
    static uint32_t last_temp_check = 0;
    uint32_t now = millis();
    if (now - last_temp_check >= 100) {
        last_temp_check = now;
        
        adc_select_input(4);
        uint16_t val = adc_read();
        float voltage = val * 3.3f / 4095.0f;
        g_status.temperature = 27.0f - (voltage - 0.706f) / 0.001721f;
    }
    
    delay(1);
}