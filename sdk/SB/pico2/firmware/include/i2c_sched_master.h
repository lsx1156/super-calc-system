#ifndef _I2C_SCHED_MASTER_H
#define _I2C_SCHED_MASTER_H

#include <stdint.h>
#include <stdbool.h>
#include "i2c_sched_regs.h"

// -------------------- 调度状态 --------------------
typedef enum {
    SCHED_IDLE = 0,
    SCHED_INITIALIZING,
    SCHED_READY,
    SCHED_RUNNING,
    SCHED_ERROR
} sched_state_t;

// -------------------- I2C主机配置 --------------------
typedef struct {
    i2c_inst_t* i2c_port;
    uint8_t sda_pin;
    uint8_t scl_pin;
    uint32_t bus_freq;
    uint8_t max_nodes;
} i2c_master_config_t;

// -------------------- 节点状态 --------------------
typedef struct {
    uint8_t node_id;
    uint8_t cluster_id;
    bool online;
    bool sync_locked;
    uint8_t status;
    uint8_t error;
    uint8_t version;
    i2c_regs_t cached_regs;
} i2c_node_state_t;

// -------------------- 调度上下文 --------------------
typedef struct {
    sched_state_t state;
    i2c_master_config_t config;
    i2c_node_state_t nodes[MAX_PICO_SLAVES];
    uint8_t node_count;
    sync_context_t sync_ctx;
    
    uint32_t last_sync_time;
    uint32_t sync_interval_us;
    
    uint8_t broadcast_buffer[32];
    uint8_t response_buffer[64];
} i2c_sched_ctx_t;

// -------------------- 函数声明 --------------------

// 初始化I2C调度主机
i2c_status_t i2c_sched_master_init(const i2c_master_config_t* config);

// 读取指定节点的寄存器
i2c_status_t i2c_sched_read_reg(uint8_t node_id, uint8_t reg_addr, uint8_t* value);

// 写入指定节点的寄存器
i2c_status_t i2c_sched_write_reg(uint8_t node_id, uint8_t reg_addr, uint8_t value);

// 批量读取多个寄存器
i2c_status_t i2c_sched_read_regs(uint8_t node_id, uint8_t start_addr, 
                                  uint8_t* data, uint8_t count);

// 批量写入多个寄存器
i2c_status_t i2c_sched_write_regs(uint8_t node_id, uint8_t start_addr, 
                                   const uint8_t* data, uint8_t count);

// 广播写入（所有节点）
i2c_status_t i2c_sched_broadcast_write(uint8_t reg_addr, uint8_t value);

// 广播写入多个寄存器
i2c_status_t i2c_sched_broadcast_write_regs(uint8_t start_addr, 
                                             const uint8_t* data, uint8_t count);

// 触发同步采样
i2c_status_t i2c_sched_trigger_sync(void);

// 节点检测
i2c_status_t i2c_sched_detect_nodes(void);

// 获取节点状态
const i2c_node_state_t* i2c_sched_get_node_state(uint8_t node_id);

// 获取调度器状态
sched_state_t i2c_sched_get_state(void);

// 启动调度
i2c_status_t i2c_sched_start(void);

// 停止调度
i2c_status_t i2c_sched_stop(void);

// 同步更新所有节点状态
void i2c_sched_sync_update(void);

// 关闭I2C调度主机
void i2c_sched_master_deinit(void);

#endif
