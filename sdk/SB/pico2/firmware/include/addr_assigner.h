#ifndef _ADDR_ASSIGNER_H
#define _ADDR_ASSIGNER_H

#include <stdint.h>
#include <stdbool.h>
#include "addr_assign_protocol.h"

// -------------------- 分配器状态 --------------------
typedef enum {
    ASSIGNER_IDLE = 0,
    ASSIGNER_SCANNING,
    ASSIGNER_COMPLETE,
    ASSIGNER_ERROR
} assigner_state_t;

// -------------------- 分配器上下文 --------------------
typedef struct {
    assigner_state_t state;
    slot_mapping_t slots[MAX_SLOT_COUNT];
    uint8_t assigned_count;
    uint8_t total_detected;
    uint32_t scan_start_time;
    uint32_t scan_duration_ms;
} addr_assigner_ctx_t;

// -------------------- 函数声明 --------------------

// 初始化地址分配器
void addr_assigner_init(void);

// 执行全槽位扫描分配
uint8_t addr_assigner_scan_all(void);

// 对指定槽位执行分配
bool addr_assigner_assign_slot(uint8_t slot_id);

// 查询指定槽位状态
bool addr_assigner_query_slot(uint8_t slot_id, slot_mapping_t* mapping);

// 清除指定槽位地址
bool addr_assigner_clear_slot(uint8_t slot_id);

// 获取槽位映射表
const slot_mapping_t* addr_assigner_get_mapping(void);

// 获取已分配节点数
uint8_t addr_assigner_get_assigned_count(void);

// 获取分配器状态
assigner_state_t addr_assigner_get_state(void);

// 重新扫描未分配槽位
uint8_t addr_assigner_rescan_unassigned(void);

// 检查所有槽位是否完成分配
bool addr_assigner_all_assigned(void);

#endif
