#include "spi_comm.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

static spi_cmd_t g_cmd;
static int g_dma_rx_chan = -1;
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

static void spi_cs_irq_handler(uint gpio, uint32_t events) {
    if (events & GPIO_IRQ_EDGE_FALL) {
        g_cs_active = true;
        g_rx_pos = 0;
        g_cs_last_low = time_us_32();
    } else if (events & GPIO_IRQ_EDGE_RISE) {
        g_cs_active = false;
        if (g_rx_pos >= 7 && g_rx_buf[g_rx_pos - 1] == FRAME_TAIL) {
            g_rx_frame_ready = true;
        }
    }
}

void spi_comm_init(void) {
    spi_init(SPI_PORT, SPI_BAUDRATE);
    spi_set_slave(SPI_PORT, true);
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    
    gpio_set_function(SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MISO_PIN, GPIO_FUNC_SPI);
    
    gpio_init(SPI_CS_PIN);
    gpio_set_dir(SPI_CS_PIN, GPIO_IN);
    gpio_pull_up(SPI_CS_PIN);
    gpio_set_irq_enabled_with_callback(SPI_CS_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, spi_cs_irq_handler);
    
    g_cmd.cmd_ready = false;
    g_cmd.resp_len = 0;
    g_rx_pos = 0;
    g_rx_frame_ready = false;
    g_cs_active = false;
    
    g_dma_rx_chan = dma_claim_unused_channel(true);
    dma_channel_config dma_rx_cfg = dma_channel_get_default_config(g_dma_rx_chan);
    channel_config_set_transfer_data_size(&dma_rx_cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&dma_rx_cfg, false);
    channel_config_set_write_increment(&dma_rx_cfg, true);
    channel_config_set_dreq(&dma_rx_cfg, spi_get_dreq(SPI_PORT, false));
    
    dma_channel_configure(
        g_dma_rx_chan,
        &dma_rx_cfg,
        g_rx_buf,
        &spi_get_hw(SPI_PORT)->dr,
        sizeof(g_rx_buf),
        true
    );
}

void spi_comm_task(void* pvParameters) {
    while (1) {
        if (g_rx_frame_ready) {
            g_rx_frame_ready = false;
            
            if (parse_frame(g_rx_buf, g_rx_pos)) {
                g_cmd.cmd_ready = true;
            }
            
            g_rx_pos = 0;
        } else if (g_cs_active) {
            uint32_t now = time_us_32();
            if (now - g_cs_last_low > 10000) {
                g_cs_active = false;
            }
            
            uint32_t dma_written = sizeof(g_rx_buf) - dma_channel_hw_addr(g_dma_rx_chan)->transfer_count;
            if (dma_written > g_rx_pos) {
                g_rx_pos = dma_written;
                g_cs_last_low = now;
                
                if (g_rx_pos > 0 && g_rx_buf[g_rx_pos - 1] == FRAME_TAIL) {
                    g_rx_frame_ready = true;
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
            
            while (!spi_is_writable(SPI_PORT)) {
                sleep_us(1);
            }
            
            for (uint16_t i = 0; i < frame_len; i++) {
                spi_get_hw(SPI_PORT)->dr = tx_buf[i];
                while (!(spi_get_hw(SPI_PORT)->sr & SPI_SSPSR_TNF_BITS)) {
                    sleep_us(1);
                }
            }
            
            g_cmd.resp_len = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1));
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