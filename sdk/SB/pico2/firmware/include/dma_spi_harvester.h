#ifndef _DMA_SPI_HARVESTER_H
#define _DMA_SPI_HARVESTER_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/dma.h"
#include "config.h"

// ==================== 架构配置 ====================
#define DMA_SPI_BUF_SIZE     1024
#define DMA_SPI_BUF_COUNT    4
#define DMA_SPI_FRAME_SIZE   64
#define DMA_SPI_MAX_PICO     MAX_PICO_SLAVES

// ==================== DMA事件类型 ====================
typedef enum {
    DMA_EVENT_NONE = 0,
    DMA_EVENT_BUFFER_FULL,
    DMA_EVENT_FRAME_READY,
    DMA_EVENT_ERROR,
    DMA_EVENT_NODE_TIMEOUT
} dma_spi_event_t;

// ==================== Pico节点缓冲区 ====================
typedef struct {
    uint8_t buffer[DMA_SPI_BUF_SIZE];
    volatile uint32_t read_idx;
    volatile uint32_t write_idx;
    volatile uint32_t frame_count;
    
    uint8_t request_frame[32];
    uint8_t request_len;
    
    dma_channel_config dma_cfg;
    int dma_chan;
    
    volatile dma_spi_event_t pending_event;
    volatile bool dma_active;
    volatile bool node_online;
    
    uint32_t last_active;
    uint32_t error_count;
    uint32_t frame_timestamp;
} dma_spi_node_t;

// ==================== 收割器状态 ====================
typedef struct {
    dma_spi_node_t nodes[DMA_SPI_MAX_PICO];
    volatile bool running;
    volatile uint32_t total_frames;
    volatile uint32_t dropped_frames;
    
    uint8_t cs_pins[DMA_SPI_MAX_PICO];
    
    uint32_t poll_interval_us;
    uint8_t current_node;
} dma_spi_harvester_t;

// ==================== 函数声明 ====================

// 初始化DMA SPI收割器
void dma_spi_harvester_init(void);

// 启动收割
void dma_spi_harvester_start(void);

// 停止收割
void dma_spi_harvester_stop(void);

// SPI总线互斥：获取SPI总线控制权（停止DMA）
bool dma_spi_harvester_acquire_spi(void);

// SPI总线互斥：释放SPI总线控制权（恢复DMA）
void dma_spi_harvester_release_spi(void);

// 设置请求帧
void dma_spi_harvester_set_request(uint8_t node_id, const uint8_t* frame, uint8_t len);

// 获取节点数据
uint16_t dma_spi_harvester_get_data(uint8_t node_id, uint8_t* buf, uint16_t max_len);

// 检查节点事件
dma_spi_event_t dma_spi_harvester_check_event(uint8_t node_id);

// 清除节点事件
void dma_spi_harvester_clear_event(uint8_t node_id);

// 获取节点在线状态
bool dma_spi_harvester_get_node_online(uint8_t node_id);

// 设置节点在线状态
void dma_spi_harvester_set_node_online(uint8_t node_id, bool online);

// 获取节点帧时间戳
uint32_t dma_spi_harvester_get_frame_timestamp(uint8_t node_id);

// 获取收割器状态
dma_spi_harvester_t* dma_spi_harvester_get_state(void);

// DMA中断处理
void dma_spi_harvester_irq_handler(void);

// 轮询调度任务
void dma_spi_harvester_task(void* pvParameters);

#endif
