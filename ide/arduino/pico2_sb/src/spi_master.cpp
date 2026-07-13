#include "spi_master.h"
#include <SPI.h>

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

bool spi_master_init(void) {
    SPI.setRX(SPI_MISO_PIN);
    SPI.setTX(SPI_MOSI_PIN);
    SPI.setSCK(SPI_SCK_PIN);
    
    for (int i = 0; i < MAX_PICO_SLAVES; i++) {
        pinMode(SPI_CS_BASE_PIN + i, OUTPUT);
        digitalWrite(SPI_CS_BASE_PIN + i, HIGH);
    }
    
    SPI.begin();
    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE0);
    SPI.setClockDivider(SPI_CLOCK_DIV2);
    
    return true;
}

bool spi_master_send_cmd(uint8_t slave_id, uint8_t cmd, const uint8_t* params, uint8_t param_len,
                         uint8_t* resp_buf, uint16_t* resp_len) {
    if (slave_id >= MAX_PICO_SLAVES) return false;
    
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

void spi_master_broadcast(uint8_t cmd, const uint8_t* params, uint8_t param_len) {
    for (int i = 0; i < MAX_PICO_SLAVES; i++) {
        spi_master_send_cmd(i, cmd, params, param_len, NULL, NULL);
        delayMicroseconds(10);
    }
}