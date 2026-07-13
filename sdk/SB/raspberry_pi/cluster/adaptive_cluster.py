#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
自适应集群管理器
- 动态节点检测
- 故障自动恢复
- 负载均衡调度
- 热插拔支持
- 弹性扩展
"""

import time
import struct
import threading
from typing import Dict, List, Optional, Callable

from core.config import (
    MAX_PICO2_COUNT, MAX_PICO_PER_PICO2, MAX_TOTAL_PICO,
    AUTO_DETECT_NODES, AUTO_HEAL_ENABLED, AUTO_SCALE_ENABLED,
    MAX_RETRY_COUNT, HEAL_CHECK_INTERVAL, CMD, DATA_TYPE,
)
from core.logger import get_logger
from core.status_manager import status_mgr
from core.usb_comm import usb_mgr

logger = get_logger("ClusterManager")


class PicoNode:
    """单个Pico节点"""
    
    def __init__(self, pico2_id: int, node_id: int):
        self.pico2_id = pico2_id
        self.node_id = node_id
        self.online = False
        self.fault = False
        self.fault_code = 0
        self.error_count = 0
        self.last_heartbeat = 0
        self.work_mode = 0
        self.run_status = 0
        self.temperature = 25.0
        self.overclock_freq = 133000
        self.sample_count = 0
        self.utilization = 0.0  # 利用率 0-100%
    
    def to_dict(self):
        return {
            "pico2_id": self.pico2_id,
            "node_id": self.node_id,
            "online": self.online,
            "fault": self.fault,
            "fault_code": self.fault_code,
            "temperature": self.temperature,
            "utilization": self.utilization,
        }


class Pico2Node:
    """Pico2协处理器节点"""
    
    def __init__(self, pico2_id: int):
        self.id = pico2_id
        self.online = False
        self.fault = False
        self.pico_count = 0
        self.online_count = 0
        self.fault_count = 0
        self.temperature = 25.0
        self.overclock_freq = 150000
        self.nodes: Dict[int, PicoNode] = {}
    
    def to_dict(self):
        return {
            "id": self.id,
            "online": self.online,
            "pico_count": self.pico_count,
            "online_count": self.online_count,
            "fault_count": self.fault_count,
            "temperature": self.temperature,
            "nodes": {k: v.to_dict() for k, v in self.nodes.items()},
        }


class AdaptiveClusterManager:
    """自适应集群管理器"""
    
    def __init__(self):
        self.pico2_nodes: Dict[int, Pico2Node] = {}
        self._lock = threading.RLock()
        self._running = False
        self._thread: Optional[threading.Thread] = None
        self._last_heal_time = 0
        self._callbacks = {
            "node_online": [],
            "node_offline": [],
            "node_fault": [],
            "cluster_change": [],
        }
    
    def register_callback(self, event: str, callback: Callable):
        """注册事件回调"""
        if event in self._callbacks:
            self._callbacks[event].append(callback)
    
    def _fire_event(self, event: str, *args, **kwargs):
        """触发事件"""
        if event in self._callbacks:
            for cb in self._callbacks[event]:
                try:
                    cb(*args, **kwargs)
                except Exception as e:
                    logger.error(f"回调异常 {event}: {e}")
    
    def init(self):
        """初始化集群"""
        for i in range(MAX_PICO2_COUNT):
            self.pico2_nodes[i] = Pico2Node(i)
        
        if AUTO_DETECT_NODES:
            self.detect_all()
        
        self._running = True
        self._thread = threading.Thread(target=self._monitor_loop, daemon=True)
        self._thread.start()
        
        logger.info("自适应集群管理器初始化完成")
    
    def detect_all(self) -> int:
        """检测所有节点"""
        total_online = 0
        
        for pico2_id, pico2 in self.pico2_nodes.items():
            if not self._is_pico2_connected(pico2_id):
                continue
            
            # 检测Pico2下的Pico节点
            online = self._detect_pico2_nodes(pico2_id)
            total_online += online
            pico2.online = True
            pico2.online_count = online
        
        self._update_cluster_status()
        self._fire_event("cluster_change")
        
        logger.info(f"集群检测完成: {total_online} 个Pico节点在线")
        return total_online
    
    def _is_pico2_connected(self, pico2_id: int) -> bool:
        """检查Pico2是否连接"""
        for dev in usb_mgr.devices:
            if dev.id == pico2_id and dev.connected:
                return True
        return False
    
    def _detect_pico2_nodes(self, pico2_id: int) -> int:
        """检测某个Pico2下的所有Pico节点"""
        # 发送节点检测命令
        params = struct.pack("<B", 0)
        dev = self._get_pico2_device(pico2_id)
        if not dev:
            return 0
        
        dev.send_command(CMD["NODE_DETECT"], params)
        time.sleep(0.2)
        
        # 尝试读取响应
        online_count = 0
        for _ in range(10):
            frame = dev.read_frame()
            if frame and frame[0] == DATA_TYPE["STATUS"]:
                data = frame[1]
                if len(data) > 2:
                    online_count = data[1] if len(data) > 1 else 0
                break
            time.sleep(0.05)
        
        # 初始化节点对象
        pico2 = self.pico2_nodes[pico2_id]
        pico2.pico_count = 8  # 默认8个，可动态扩展
        for i in range(pico2.pico_count):
            if i not in pico2.nodes:
                pico2.nodes[i] = PicoNode(pico2_id, i)
        
        return online_count
    
    def _get_pico2_device(self, pico2_id: int):
        """获取Pico2设备对象"""
        for dev in usb_mgr.devices:
            if dev.id == pico2_id:
                return dev
        return None
    
    def _monitor_loop(self):
        """监控循环"""
        while self._running:
            try:
                # 心跳检测
                self._heartbeat_check()
                
                # 故障自愈
                if AUTO_HEAL_ENABLED:
                    now = time.time()
                    if now - self._last_heal_time > HEAL_CHECK_INTERVAL:
                        self._auto_heal()
                        self._last_heal_time = now
                
                # 负载均衡
                if AUTO_SCALE_ENABLED:
                    self._load_balance()
                
                # 更新状态
                self._update_cluster_status()
                
            except Exception as e:
                logger.error(f"集群监控异常: {e}")
            
            time.sleep(1)
    
    def _heartbeat_check(self):
        """心跳检测"""
        now = time.time()
        
        for pico2_id, pico2 in self.pico2_nodes.items():
            if not pico2.online:
                continue
            
            for node_id, node in pico2.nodes.items():
                if not node.online:
                    continue
                
                # 超时检测（30秒无心跳视为离线）
                if now - node.last_heartbeat > 30:
                    node.online = False
                    node.fault = True
                    logger.warning(f"节点超时离线: Pico2#{pico2_id} Pico#{node_id}")
                    self._fire_event("node_offline", pico2_id, node_id)
    
    def _auto_heal(self) -> int:
        """故障自愈，返回恢复的节点数"""
        healed = 0
        
        for pico2_id, pico2 in self.pico2_nodes.items():
            if not pico2.online:
                # 尝试重连Pico2
                dev = self._get_pico2_device(pico2_id)
                if dev and not dev.connected:
                    if dev.reconnect():
                        pico2.online = True
                        healed += 1
                        logger.info(f"Pico2 #{pico2_id} 重连成功")
                continue
            
            for node_id, node in pico2.nodes.items():
                if node.fault and not node.online:
                    if self._reset_node(pico2_id, node_id):
                        node.fault = False
                        node.error_count = 0
                        healed += 1
                        logger.info(f"节点自愈: Pico2#{pico2_id} Pico#{node_id}")
        
        if healed > 0:
            self._update_cluster_status()
            self._fire_event("cluster_change")
        
        return healed
    
    def _reset_node(self, pico2_id: int, node_id: int) -> bool:
        """复位单个节点"""
        dev = self._get_pico2_device(pico2_id)
        if not dev:
            return False
        
        params = struct.pack("<B", node_id)
        dev.send_command(CMD["RESET_NODE"], params)
        time.sleep(0.5)
        
        # 重新检测
        return True
    
    def _load_balance(self):
        """负载均衡"""
        # 计算各节点利用率，调整任务分配
        pass  # TODO: 实现负载均衡策略
    
    def _update_cluster_status(self):
        """更新集群状态到全局状态管理器"""
        total_pico = 0
        online_pico = 0
        fault_pico = 0
        pico2_online = 0
        
        for pico2 in self.pico2_nodes.values():
            if pico2.online:
                pico2_online += 1
            total_pico += pico2.pico_count
            online_pico += pico2.online_count
            fault_pico += pico2.fault_count
        
        status_mgr.update_cluster(
            pico2_count=len(self.pico2_nodes),
            pico2_online=pico2_online,
            total_pico=total_pico,
            online_pico=online_pico,
            fault_pico=fault_pico,
            pico2_devices=[p.to_dict() for p in self.pico2_nodes.values() if p.online],
        )
    
    def set_node_count(self, pico2_id: int, count: int) -> bool:
        """设置Pico2下的Pico数量（热扩展）"""
        if count < 1 or count > MAX_PICO_PER_PICO2:
            return False
        
        dev = self._get_pico2_device(pico2_id)
        if not dev or not dev.connected:
            return False
        
        params = struct.pack("<B", count)
        if dev.send_command(CMD["SET_NODE_COUNT"], params):
            pico2 = self.pico2_nodes[pico2_id]
            pico2.pico_count = count
            
            # 新增/移除节点
            for i in range(count):
                if i not in pico2.nodes:
                    pico2.nodes[i] = PicoNode(pico2_id, i)
            
            keys_to_remove = [k for k in pico2.nodes if k >= count]
            for k in keys_to_remove:
                del pico2.nodes[k]
            
            logger.info(f"Pico2 #{pico2_id} 节点数调整为 {count}")
            self._update_cluster_status()
            return True
        
        return False
    
    def get_online_nodes(self) -> List[PicoNode]:
        """获取所有在线节点"""
        nodes = []
        for pico2 in self.pico2_nodes.values():
            for node in pico2.nodes.values():
                if node.online:
                    nodes.append(node)
        return nodes
    
    def get_total_online(self) -> int:
        """获取在线Pico总数"""
        return sum(p.online_count for p in self.pico2_nodes.values())
    
    def stop(self):
        """停止集群管理"""
        self._running = False
        if self._thread:
            self._thread.join(timeout=2)
        logger.info("集群管理器已停止")


# 全局实例
cluster_mgr = AdaptiveClusterManager()
