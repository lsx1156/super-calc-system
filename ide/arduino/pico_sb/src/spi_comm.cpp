#include "spi_comm.h"

static spi_cmd_t g_cmd;
static uint8_t g_rx_buf[256];
static uint16_t g_rx_pos = 0;
static volatile bool g_rx_frame_ready = false;
static volatile bool g_cs_active = false;
static uint32_t g_cs_last_low = 0;

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

static bool parse_frame(const uint8_t* rx_buf, uint32_t len) {
    if (len < 7) return false;
    if (rx_buf[0] != FRAME_HEADER) return false;
    if (rx_buf[len-1] != FRAME_TAIL) return false;
    
    uint16_t data_len = rx_buf[1] | (rx_buf[2] << 8);
    if (data_len + 6 > len) return false;
    
    uint16_t crc_rx = rx_buf[3 + data_len] | (rx_buf[4 + data_len] << 8);
    uint16_t crc_calc = crc16(&rx_buf[3], data_len);
    if (crc_rx != crc_calc) return false;
    
    g_cmd.cmd = rx_buf[3];
    g_cmd.param_len = data_len - 1;
    if (g_cmd.param_len > 0) {
        memcpy(g_cmd.params, &rx_buf[4], g_cmd.param_len);
    }
    g_cmd.cmd_ready = true;
    
    return true;
}

void spi_comm_init(void) {
    SPI.setRX(SPI_MISO_PIN);
    SPI.setTX(SPI_MOSI_PIN);
    SPI.setSCK(SPI_SCK_PIN);
    pinMode(SPI_CS_PIN, INPUT_PULLUP);
    
    SPI.begin();
    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE0);
    SPI.setClockDivider(SPI_CLOCK_DIV2);
    
    g_cmd.cmd_ready = false;
    g_cmd.resp_len = 0;
    g_rx_pos = 0;
    g_rx_frame_ready = false;
    g_cs_active = false;
}

void spi_comm_task(void) {
    bool cs_low = !digitalRead(SPI_CS_PIN);
    
    if (cs_low && !g_cs_active) {
        g_cs_active = true;
        g_rx_pos = 0;
        g_cs_last_low = micros();
    } else if (!cs_low && g_cs_active) {
        g_cs_active = false;
        if (g_rx_pos >= 7 && g_rx_buf[g_rx_pos - 1] == FRAME_TAIL) {
            g_rx_frame_ready = true;
        }
    }
    
    if (g_rx_frame_ready) {
        g_rx_frame_ready = false;
        if (parse_frame(g_rx_buf, g_rx_pos)) {
            g_cmd.cmd_ready = true;
        }
        g_rx_pos = 0;
    } else if (g_cs_active) {
        uint32_t now = micros();
        if (now - g_cs_last_low > 10000) {
            g_cs_active = false;
        }
        
        while (SPI.available()) {
            if (g_rx_pos < sizeof(g_rx_buf)) {
                g_rx_buf[g_rx_pos++] = SPI.read();
                g_cs_last_low = now;
                
                if (g_rx_pos > 0 && g_rx_buf[g_rx_pos - 1] == FRAME_TAIL) {
                    g_rx_frame_ready = true;
                }
            }
        }
    }
    
    if (g_cmd.resp_len > 0) {
        uint16_t frame_len = 6 + g_cmd.resp_len;
        uint8_t tx_buf[128];
        
        tx_buf[0] = FRAME_HEADER;
        tx_buf[1] = g_cmd.resp_len & 0xFF;
        tx_buf[2] = (g_cmd.resp_len >> 8) & 0xFF;
        memcpy(&tx_buf[3], g_cmd.resp, g_cmd.resp_len);
        uint16_t crc = crc16(g_cmd.resp, g_cmd.resp_len);
        tx_buf[3 + g_cmd.resp_len] = crc & 0xFF;
        tx_buf[4 + g_cmd.resp_len] = (crc >> 8) & 0xFF;
        tx_buf[5 + g_cmd.resp_len] = FRAME_TAIL;
        
        for (uint16_t i = 0; i < frame_len; i++) {
            SPI.transfer(tx_buf[i]);
        }
        
        g_cmd.resp_len = 0;
    }
}

bool spi_comm_get_cmd(spi_cmd_t* cmd) {
    if (!g_cmd.cmd_ready) return false;
    memcpy(cmd, &g_cmd, sizeof(spi_cmd_t));
    g_cmd.cmd_ready = false;
    return true;
}

void spi_comm_send_resp(const uint8_t* data, uint16_t len) {
    if (len > 128) len = 128;
    memcpy(g_cmd.resp, data, len);
    g_cmd.resp_len = len;
}