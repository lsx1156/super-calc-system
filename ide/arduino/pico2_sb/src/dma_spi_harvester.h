#ifndef DMA_SPI_HARVESTER_H
#define DMA_SPI_HARVESTER_H

#include <Arduino.h>

#define DMA_SPI_BUF_SIZE     1024
#define DMA_SPI_BUF_COUNT    4
#define DMA_SPI_FRAME_SIZE   64
#define DMA_SPI_MAX_PICO     MAX_PICO_SLAVES

typedef enum {
    DMA_EVENT_NONE = 0,
    DMA_EVENT_BUFFER_FULL,
    DMA_EVENT_FRAME_READY,
    DMA_EVENT_ERROR,
    DMA_EVENT_NODE_TIMEOUT
} dma_spi_event_t;

typedef struct {
    uint8_t buffer[DMA_SPI_BUF_SIZE];
    uint32_t read_idx;
    uint32_t write_idx;
    uint32_t frame_count;
    
    uint8_t request_frame[32];
    uint8_t request_len;
    
    volatile dma_spi_event_t pending_event;
    volatile bool dma_active;
    volatile bool node_online;
    
    uint32_t last_active;
    uint32_t error_count;
    uint32_t frame_timestamp;
} dma_spi_node_t;

typedef struct {
    dma_spi_node_t nodes[DMA_SPI_MAX_PICO];
    bool running;
    uint32_t total_frames;
    uint32_t dropped_frames;
    
    uint8_t cs_pins[DMA_SPI_MAX_PICO];
    
    uint32_t poll_interval_us;
    uint8_t current_node;
} dma_spi_harvester_t;

void dma_spi_harvester_init(void);
void dma_spi_harvester_start(void);
void dma_spi_harvester_stop(void);
bool dma_spi_harvester_acquire_spi(void);
void dma_spi_harvester_release_spi(void);
void dma_spi_harvester_set_request(uint8_t node_id, const uint8_t* frame, uint8_t len);
uint16_t dma_spi_harvester_get_data(uint8_t node_id, uint8_t* buf, uint16_t max_len);
dma_spi_event_t dma_spi_harvester_check_event(uint8_t node_id);
void dma_spi_harvester_clear_event(uint8_t node_id);
bool dma_spi_harvester_get_node_online(uint8_t node_id);
void dma_spi_harvester_set_node_online(uint8_t node_id, bool online);
uint32_t dma_spi_harvester_get_frame_timestamp(uint8_t node_id);
dma_spi_harvester_t* dma_spi_harvester_get_state(void);

#endif