#include "dma_spi_harvester.h"
#include <SPI.h>
#include <string.h>

static dma_spi_harvester_t g_harvester;
static bool g_spi_initialized = false;
static bool g_dma_was_running = false;

void dma_spi_harvester_init(void) {
    memset(&g_harvester, 0, sizeof(g_harvester));
    
    if (!g_spi_initialized) {
        SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);
        SPI.setClockDivider(SPI_CLOCK_DIV2);
        
        g_spi_initialized = true;
    }
    
    for (int i = 0; i < DMA_SPI_MAX_PICO; i++) {
        g_harvester.cs_pins[i] = SPI_CS_BASE_PIN + i;
        pinMode(g_harvester.cs_pins[i], OUTPUT);
        digitalWrite(g_harvester.cs_pins[i], HIGH);
        
        memset(g_harvester.nodes[i].buffer, 0, DMA_SPI_BUF_SIZE);
        g_harvester.nodes[i].read_idx = 0;
        g_harvester.nodes[i].write_idx = 0;
        g_harvester.nodes[i].frame_count = 0;
        g_harvester.nodes[i].request_len = 0;
        memset(g_harvester.nodes[i].request_frame, 0, sizeof(g_harvester.nodes[i].request_frame));
        g_harvester.nodes[i].pending_event = DMA_EVENT_NONE;
        g_harvester.nodes[i].dma_active = false;
        g_harvester.nodes[i].node_online = true;
        g_harvester.nodes[i].last_active = micros();
        g_harvester.nodes[i].error_count = 0;
        g_harvester.nodes[i].frame_timestamp = 0;
    }
    
    g_harvester.poll_interval_us = 0;
    g_harvester.current_node = 0;
    g_harvester.running = false;
}

void dma_spi_harvester_start(void) {
    g_harvester.running = true;
}

void dma_spi_harvester_stop(void) {
    g_harvester.running = false;
    
    for (int i = 0; i < DMA_SPI_MAX_PICO; i++) {
        if (g_harvester.nodes[i].dma_active) {
            digitalWrite(g_harvester.cs_pins[i], HIGH);
            g_harvester.nodes[i].dma_active = false;
        }
    }
}

bool dma_spi_harvester_acquire_spi(void) {
    g_dma_was_running = g_harvester.running;
    if (g_harvester.running) {
        dma_spi_harvester_stop();
    }
    return true;
}

void dma_spi_harvester_release_spi(void) {
    if (g_dma_was_running) {
        dma_spi_harvester_start();
    }
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