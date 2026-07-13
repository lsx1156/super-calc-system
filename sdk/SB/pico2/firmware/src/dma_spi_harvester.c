#include "dma_spi_harvester.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>

static dma_spi_harvester_t g_harvester;
static bool g_spi_initialized = false;
static SemaphoreHandle_t g_spi_mutex = NULL;
static bool g_dma_was_running = false;

static void dma_spi_node_init(uint8_t node_id) {
    dma_spi_node_t* node = &g_harvester.nodes[node_id];
    
    memset(node->buffer, 0, DMA_SPI_BUF_SIZE);
    node->read_idx = 0;
    node->write_idx = 0;
    node->frame_count = 0;
    
    node->request_len = 0;
    memset(node->request_frame, 0, sizeof(node->request_frame));
    
    node->dma_chan = dma_claim_unused_channel(true);
    node->dma_active = false;
    node->pending_event = DMA_EVENT_NONE;
    node->node_online = true;
    
    node->last_active = time_us_32();
    node->error_count = 0;
    
    node->dma_cfg = dma_channel_get_default_config(node->dma_chan);
    channel_config_set_transfer_data_size(&node->dma_cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&node->dma_cfg, false);
    channel_config_set_write_increment(&node->dma_cfg, true);
    channel_config_set_dreq(&node->dma_cfg, spi_get_dreq(SPI_PORT, false));
    
    dma_channel_set_irq0_enabled(node->dma_chan, true);
}

static void dma_spi_start_transfer(uint8_t node_id) {
    dma_spi_node_t* node = &g_harvester.nodes[node_id];
    if (!node->node_online || node->dma_active) return;
    
    gpio_put(g_harvester.cs_pins[node_id], 0);
    
    dma_channel_configure(
        node->dma_chan,
        &node->dma_cfg,
        &node->buffer[node->write_idx % DMA_SPI_BUF_SIZE],
        &spi_get_hw(SPI_PORT)->dr,
        DMA_SPI_FRAME_SIZE,
        false
    );
    
    dma_start_channel_mask(1u << node->dma_chan);
    node->dma_active = true;
}

static void dma_spi_complete_transfer(uint8_t node_id) {
    dma_spi_node_t* node = &g_harvester.nodes[node_id];
    uint32_t now = time_us_32();
    
    gpio_put(g_harvester.cs_pins[node_id], 1);
    
    node->write_idx += DMA_SPI_FRAME_SIZE;
    node->frame_count++;
    g_harvester.total_frames++;
    
    if (node->write_idx - node->read_idx > DMA_SPI_BUF_SIZE) {
        node->read_idx = node->write_idx - DMA_SPI_BUF_SIZE;
        g_harvester.dropped_frames++;
    }
    
    node->frame_timestamp = now;
    node->pending_event = DMA_EVENT_FRAME_READY;
    node->dma_active = false;
    node->last_active = now;
    
    if (g_harvester.running) {
        uint8_t next_node = (node_id + 1) % DMA_SPI_MAX_PICO;
        dma_spi_start_transfer(next_node);
    }
}

void dma_spi_harvester_irq_handler(void) {
    for (int i = 0; i < DMA_SPI_MAX_PICO; i++) {
        dma_spi_node_t* node = &g_harvester.nodes[i];
        
        if (dma_channel_get_irq0_status(node->dma_chan)) {
            dma_channel_clear_irq0_status(node->dma_chan);
            
            if (node->dma_active) {
                dma_spi_complete_transfer(i);
            }
        }
    }
}

void dma_spi_harvester_init(void) {
    memset(&g_harvester, 0, sizeof(g_harvester));
    
    if (!g_spi_initialized) {
        spi_init(SPI_PORT, SPI_BAUDRATE);
        spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        
        gpio_set_function(SPI_MOSI_PIN, GPIO_FUNC_SPI);
        gpio_set_function(SPI_MISO_PIN, GPIO_FUNC_SPI);
        gpio_set_function(SPI_SCK_PIN, GPIO_FUNC_SPI);
        
        g_spi_initialized = true;
    }
    
    if (g_spi_mutex == NULL) {
        g_spi_mutex = xSemaphoreCreateMutex();
    }
    
    for (int i = 0; i < DMA_SPI_MAX_PICO; i++) {
        g_harvester.cs_pins[i] = SPI_CS_BASE_PIN + i;
        gpio_init(g_harvester.cs_pins[i]);
        gpio_set_dir(g_harvester.cs_pins[i], GPIO_OUT);
        gpio_put(g_harvester.cs_pins[i], 1);
        
        dma_spi_node_init(i);
    }
    
    irq_set_exclusive_handler(DMA_IRQ_0, dma_spi_harvester_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    
    g_harvester.poll_interval_us = 0;
    g_harvester.current_node = 0;
    g_harvester.running = false;
}

void dma_spi_harvester_start(void) {
    g_harvester.running = true;
    
    for (int i = 0; i < DMA_SPI_MAX_PICO; i++) {
        dma_spi_node_t* node = &g_harvester.nodes[i];
        if (node->node_online) {
            dma_spi_start_transfer(i);
            break;
        }
    }
}

void dma_spi_harvester_stop(void) {
    g_harvester.running = false;
    
    for (int i = 0; i < DMA_SPI_MAX_PICO; i++) {
        dma_spi_node_t* node = &g_harvester.nodes[i];
        if (node->dma_active) {
            dma_channel_abort(node->dma_chan);
            gpio_put(g_harvester.cs_pins[i], 1);
            node->dma_active = false;
        }
    }
}

bool dma_spi_harvester_acquire_spi(void) {
    if (g_spi_mutex == NULL) return false;
    
    if (xSemaphoreTake(g_spi_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }
    
    g_dma_was_running = g_harvester.running;
    if (g_harvester.running) {
        dma_spi_harvester_stop();
    }
    
    return true;
}

void dma_spi_harvester_release_spi(void) {
    if (g_spi_mutex == NULL) return;
    
    if (g_dma_was_running) {
        dma_spi_harvester_start();
    }
    
    xSemaphoreGive(g_spi_mutex);
}

void dma_spi_harvester_set_request(uint8_t node_id, const uint8_t* frame, uint8_t len) {
    if (node_id >= DMA_SPI_MAX_PICO) return;
    
    dma_spi_node_t* node = &g_harvester.nodes[node_id];
    
    if (len > sizeof(node->request_frame)) {
        len = sizeof(node->request_frame);
    }
    
    memcpy(node->request_frame, frame, len);
    node->request_len = len;
}

uint16_t dma_spi_harvester_get_data(uint8_t node_id, uint8_t* buf, uint16_t max_len) {
    if (node_id >= DMA_SPI_MAX_PICO || !buf) return 0;
    
    dma_spi_node_t* node = &g_harvester.nodes[node_id];
    
    uint32_t available = node->write_idx - node->read_idx;
    if (available == 0) return 0;
    
    uint16_t to_read = (available > max_len) ? max_len : (uint16_t)available;
    
    for (uint16_t i = 0; i < to_read; i++) {
        buf[i] = node->buffer[(node->read_idx + i) % DMA_SPI_BUF_SIZE];
    }
    
    node->read_idx += to_read;
    return to_read;
}

dma_spi_event_t dma_spi_harvester_check_event(uint8_t node_id) {
    if (node_id >= DMA_SPI_MAX_PICO) return DMA_EVENT_NONE;
    return g_harvester.nodes[node_id].pending_event;
}

void dma_spi_harvester_clear_event(uint8_t node_id) {
    if (node_id >= DMA_SPI_MAX_PICO) return;
    g_harvester.nodes[node_id].pending_event = DMA_EVENT_NONE;
}

bool dma_spi_harvester_get_node_online(uint8_t node_id) {
    if (node_id >= DMA_SPI_MAX_PICO) return false;
    return g_harvester.nodes[node_id].node_online;
}

void dma_spi_harvester_set_node_online(uint8_t node_id, bool online) {
    if (node_id >= DMA_SPI_MAX_PICO) return;
    g_harvester.nodes[node_id].node_online = online;
}

uint32_t dma_spi_harvester_get_frame_timestamp(uint8_t node_id) {
    if (node_id >= DMA_SPI_MAX_PICO) return 0;
    return g_harvester.nodes[node_id].frame_timestamp;
}

dma_spi_harvester_t* dma_spi_harvester_get_state(void) {
    return &g_harvester;
}

void dma_spi_harvester_task(void* pvParameters) {
    while (1) {
        for (int i = 0; i < DMA_SPI_MAX_PICO; i++) {
            dma_spi_node_t* node = &g_harvester.nodes[i];
            
            if (!node->node_online || !g_harvester.running) continue;
            
            if (!node->dma_active) {
                dma_spi_start_transfer(i);
            }
            
            uint32_t now = time_us_32();
            if (node->dma_active && now - node->last_active > 100000) {
                node->error_count++;
                if (node->error_count > 10) {
                    node->pending_event = DMA_EVENT_NODE_TIMEOUT;
                    dma_channel_abort(node->dma_chan);
                    gpio_put(g_harvester.cs_pins[i], 1);
                    node->dma_active = false;
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}