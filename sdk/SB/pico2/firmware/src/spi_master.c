#include "spi_master.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "dma_spi_harvester.h"
#include <string.h>

static bool g_spi_initialized = false;

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

void spi_master_init(void) {
    if (!g_spi_initialized) {
        spi_init(SPI_PORT, SPI_BAUDRATE);
        spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        
        gpio_set_function(SPI_MOSI_PIN, GPIO_FUNC_SPI);
        gpio_set_function(SPI_MISO_PIN, GPIO_FUNC_SPI);
        gpio_set_function(SPI_SCK_PIN, GPIO_FUNC_SPI);
        
        g_spi_initialized = true;
    }
    
    for (int i = 0; i < MAX_PICO_SLAVES; i++) {
        gpio_init(SPI_CS_BASE_PIN + i);
        gpio_set_dir(SPI_CS_BASE_PIN + i, GPIO_OUT);
        gpio_put(SPI_CS_BASE_PIN + i, 1);
    }
}

void spi_master_select(uint8_t node_id) {
    if (node_id < MAX_PICO_SLAVES) {
        gpio_put(SPI_CS_BASE_PIN + node_id, 0);
    }
}

void spi_master_deselect(uint8_t node_id) {
    if (node_id < MAX_PICO_SLAVES) {
        gpio_put(SPI_CS_BASE_PIN + node_id, 1);
    }
}

bool spi_master_send_cmd(uint8_t node_id, uint8_t cmd, 
                         const uint8_t* params, uint8_t param_len,
                         uint8_t* resp, uint16_t* resp_len) {
    if (node_id >= MAX_PICO_SLAVES) return false;
    
    if (!dma_spi_harvester_acquire_spi()) {
        return false;
    }
    
    uint8_t tx_buf[256];
    uint8_t rx_buf[256];
    
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
    
    uint16_t req_len = 7 + param_len;
    
    spi_master_select(node_id);
    sleep_us(1);
    
    spi_write_read_blocking(SPI_PORT, tx_buf, rx_buf, req_len);
    
    uint32_t start_time = time_us_32();
    bool resp_ok = false;
    uint16_t resp_data_len = 0;
    
    while (time_us_32() - start_time < 5000) {
        memset(tx_buf, 0, sizeof(tx_buf));
        spi_write_read_blocking(SPI_PORT, tx_buf, rx_buf, 128);
        
        if (rx_buf[0] == FRAME_HEADER_PICO && rx_buf[127] == FRAME_TAIL_PICO) {
            resp_data_len = rx_buf[1] | (rx_buf[2] << 8);
            if (resp_data_len <= 120) {
                resp_ok = true;
                break;
            }
        }
        
        sleep_us(100);
    }
    
    sleep_us(1);
    spi_master_deselect(node_id);
    
    dma_spi_harvester_release_spi();
    
    if (resp_ok && resp && resp_len && resp_data_len < 128) {
        *resp_len = resp_data_len;
        memcpy(resp, &rx_buf[3], resp_data_len);
        return true;
    }
    
    return false;
}

bool spi_master_broadcast(uint8_t cmd, const uint8_t* params, uint8_t param_len) {
    bool any_success = false;
    
    if (!dma_spi_harvester_acquire_spi()) {
        return false;
    }
    
    for (int i = 0; i < MAX_PICO_SLAVES; i++) {
        uint8_t tx_buf[256];
        uint8_t rx_buf[256];
        
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
        
        uint16_t req_len = 7 + param_len;
        
        spi_master_select(i);
        sleep_us(1);
        spi_write_read_blocking(SPI_PORT, tx_buf, rx_buf, req_len);
        sleep_us(1);
        spi_master_deselect(i);
        
        if (rx_buf[0] == FRAME_HEADER_PICO) {
            any_success = true;
        }
    }
    
    dma_spi_harvester_release_spi();
    
    return any_success;
}
