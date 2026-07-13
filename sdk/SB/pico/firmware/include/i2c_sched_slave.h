#ifndef _I2C_SCHED_SLAVE_H
#define _I2C_SCHED_SLAVE_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/i2c.h"
#include "i2c_sched_regs.h"
#include "addr_assign_protocol.h"

// -------------------- 从机配置 --------------------
typedef struct {
    i2c_inst_t* i2c_port;
    uint8_t sda_pin;
    uint8_t scl_pin;
    uint8_t node_id;
    uint8_t cluster_id;
    uint32_t bus_freq;
    bool    start_unassigned;
} i2c_slave_config_t;

// -------------------- 从机状态 --------------------
typedef enum {
    SLAVE_IDLE = 0,
    SLAVE_INITIALIZING,
    SLAVE_UNASSIGNED,
    SLAVE_READY,
    SLAVE_RUNNING,
    SLAVE_ERROR
} slave_state_t;

// -------------------- 从机上下文 --------------------
typedef struct {
    slave_state_t state;
    i2c_slave_config_t config;
    i2c_regs_t regs;
    sync_context_t sync_ctx;
    
    uint8_t current_reg_addr;
    bool addr_written;
    
    volatile sync_event_t pending_event;
    volatile bool regs_dirty;
} i2c_slave_ctx_t;

// -------------------- 回调函数类型 --------------------
typedef void (*i2c_sync_callback_t)(sync_event_t event, uint8_t reg_addr, uint8_t value);

// -------------------- 函数声明 --------------------

// 初始化I2C从机
i2c_status_t i2c_sched_slave_init(const i2c_slave_config_t* config);

// 设置同步回调函数
void i2c_sched_slave_set_callback(i2c_sync_callback_t callback);

// 获取寄存器值
uint8_t i2c_sched_slave_get_reg(uint8_t reg_addr);

// 设置寄存器值
void i2c_sched_slave_set_reg(uint8_t reg_addr, uint8_t value);

// 更新状态寄存器
void i2c_sched_slave_update_status(uint8_t status_bit, bool set);

// 更新错误寄存器
void i2c_sched_slave_update_error(uint8_t error_bit, bool set);

// 清除错误寄存器
void i2c_sched_slave_clear_error(void);

// 标记数据就绪
void i2c_sched_slave_set_data_ready(void);

// 标记同步锁定
void i2c_sched_slave_set_sync_locked(bool locked);

// 获取从机状态
slave_state_t i2c_sched_slave_get_state(void);

// 获取当前同步事件
sync_event_t i2c_sched_slave_get_event(void);

// 清除同步事件
void i2c_sched_slave_clear_event(void);

// 检查寄存器是否变更
bool i2c_sched_slave_regs_changed(void);

// 动态设置I2C从机地址（地址分配后调用）
bool i2c_sched_slave_set_address(uint8_t i2c_addr);

// 获取当前I2C地址
uint8_t i2c_sched_slave_get_address(void);

// 是否已分配地址
bool i2c_sched_slave_is_assigned(void);

// 清除地址，回到未分配状态
void i2c_sched_slave_clear_address(void);

// 关闭I2C从机
void i2c_sched_slave_deinit(void);

#endif
